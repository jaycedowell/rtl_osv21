#include "Python.h"
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "rtl-sdr.h"

// Device/detection parameters
#define FREQUENCY 433800000
#define SAMPLE_RATE 1000000
#define SMOOTH_WINDOW 488
#define RTL_BUFFER_SIZE 32768
#define THRESHOLD 6800.0

static int do_exit = 0;
static rtlsdr_dev_t *dev = NULL;

/*
  sighandler - Signal handler for the readRTL function
*/

static void sighandler(int signum)
{
	fprintf(stderr, "Signal caught, exiting!\n");
	do_exit = 1;
	rtlsdr_cancel_async(dev);
}


// Setup the variables - time control
static int tStart, tNow, diff;

// Setup the variables - power detection
static float runningSum = 0;
static float *powerBuffer;

// Setup the control loop
static int loopTimeOut = 0;
static int prevPower = 0;
static long dataCounter = 0;
static long prevEdge = -1;
static long edgeCountDiff = -1;
static long halfTime = 0;


/*
  decorder_callback - Function that receives the RTL-SDR buffer and performs 
  the Manchester decoding.  ctx is a pointer to a PyList object that is updaated
  when a new bit is found.
*/

static void decoder_callback(unsigned char *buf, uint32_t len, void *ctx) {
	int j, power, edge, addBit;
	float real, imag, instPower;
	PyObject *temp;

	if( ctx ) {
		// Exit if we are done
		if( do_exit ) {
        	return;
        }
		
		// Get the current time to figure out how long we've been running
		tNow = (int) time(NULL);
		diff = tNow - tStart;
		if( loopTimeOut && diff > loopTimeOut ) {
			do_exit = 1;
			rtlsdr_cancel_async(dev);
		}
		
		// Process the buffer
		for(j=0; j<len/2; j++) {
			//// Unpack
			real = ((float) *(buf + 2*j+0)) - 127.0;
			imag = ((float) *(buf + 2*j+1)) - 127.0;
			instPower = real*real + imag*imag;
			dataCounter += 1;
		
			//// Moving average
			runningSum += instPower - *(powerBuffer + (dataCounter-1) % SMOOTH_WINDOW);
			*(powerBuffer + (dataCounter-1) % SMOOTH_WINDOW) = instPower;
		
			//// Convert to an integer
			if( runningSum >= THRESHOLD*SMOOTH_WINDOW ) {
				power = 1;
			} else {
				power = 0;
			}
		
			//// Edge detection
			edge = power - prevPower;
			prevPower = power;
		
			//// Timing
			if( edge != 0 ) {
				if( prevEdge < 0 ) {
					prevEdge = dataCounter;
				}
				edgeCountDiff = dataCounter - prevEdge;
			}
				
			if( edge == 1 ) {
				////// Rising edge
		
				if( edgeCountDiff > 80000 ) {
					prevEdge = dataCounter;
					halfTime = 0;
					addBit = 1;
				} else if( edgeCountDiff < 200 || edgeCountDiff > 1100 ) {
					addBit = 0;
				} else if( edgeCountDiff < 615 ) {
					prevEdge = dataCounter;
					halfTime += 1;
					addBit = 1;
				} else {
					prevEdge = dataCounter;
					halfTime += 2;
					addBit = 1;
				}
			
				if( addBit && halfTime % 2 == 0 ) {
					temp = PyInt_FromLong(1);
					PyList_Append(ctx, temp);
					Py_DECREF(temp);
				}
			
			} else if( edge == -1 ) {
				////// Falling edge
			
				if( edgeCountDiff > 80000 ) {
					prevEdge = dataCounter;
					halfTime = 0;
					addBit = 1;
				} else if( edgeCountDiff < 400 || edgeCountDiff > 1400 ) {
					addBit = 0;
				} else if( edgeCountDiff < 850 ) {
					prevEdge = dataCounter;
					halfTime += 1;
					addBit = 1;
				} else {
					prevEdge = dataCounter;
					halfTime += 2;
					addBit = 1;
				}
			
				if( addBit && halfTime % 2 == 0 ) {
					temp = PyInt_FromLong(0);
					PyList_Append(ctx, temp);
					Py_DECREF(temp);
				}
			}
		}
		// end buffer processing loop
		
	}
}


/*
  verbose_device_search - Device search function from the librtlsdr library
*/

int verbose_device_search(char *s) {
	int i, device_count, device, offset;
	char *s2;
	char vendor[256], product[256], serial[256];
	device_count = rtlsdr_get_device_count();
	if (!device_count) {
		fprintf(stderr, "No supported devices found.\n");
		return -1;
	}
	fprintf(stderr, "Found %d device(s):\n", device_count);
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
	}
	fprintf(stderr, "\n");
	/* does string look like raw id number */
	device = (int)strtol(s, &s2, 0);
	if (s2[0] == '\0' && device >= 0 && device < device_count) {
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string exact match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strcmp(s, serial) != 0) {
			continue;}
		device = i;
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string prefix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strncmp(s, serial, strlen(s)) != 0) {
			continue;}
		device = i;
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string suffix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		offset = strlen(serial) - strlen(s);
		if (offset < 0) {
			continue;}
		if (strncmp(s, serial+offset, strlen(s)) != 0) {
			continue;}
		device = i;
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	fprintf(stderr, "No matching devices found.\n");
	return -1;
}


/*
  readRTL - Function for reading directly from an RTL-SDR and returning a list of
  Manchester decoded bits.
*/

static PyObject *readRTL(PyObject *self, PyObject *args) {
	PyObject *output, *bits;
	int r, i, dev_index;
	long duration;
	struct sigaction sigact;
	
	if( !PyArg_ParseTuple(args, "i", &duration) ) {
		PyErr_Format(PyExc_RuntimeError, "Invalid parameters");
		return NULL;
	}
	
	// Validate the input
	if( duration <= 0 ) {
		PyErr_Format(PyExc_ValueError, "Duration value must be greater than zero");
		return NULL;
	}
	
	// Setup the RTL SDR device
	dev_index = verbose_device_search("0");
	if( dev_index < 0 ) {
		PyErr_Format(PyExc_RuntimeError, "RTL SDR device not found");
		return NULL;
	}
	r = rtlsdr_open(&dev, (uint32_t)dev_index);
	if( r < 0 ) {
		PyErr_Format(PyExc_RuntimeError, "Cannot open RTL SDR device");
		return NULL;
	}

	// Setup the signal handler	so that we can exit the callback function
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);
	
	// Setup the radio
	r = rtlsdr_set_sample_rate(dev, SAMPLE_RATE);
	r = rtlsdr_set_center_freq(dev, FREQUENCY);
	r = rtlsdr_set_tuner_gain_mode(dev, 0);
	
	// Reset endpoint before we start reading from it (mandatory)
	r = rtlsdr_reset_buffer(dev);
	
	// Setup the output list
	bits = PyList_New(0);
	
	// Reset the loop control
	runningSum = 0;
	prevPower = 0;
	dataCounter = 0;
	prevEdge = -1;
	edgeCountDiff = -1;
	halfTime = 0;
	
	// Setup the variables - Power Detection
	powerBuffer = (float *) malloc(SMOOTH_WINDOW*sizeof(float));
	for(i=0; i<SMOOTH_WINDOW; i++) {
		*(powerBuffer + i) = 0.0;
	}
	
	// Setup the raw data buffer
	unsigned char *raw;
	raw = (unsigned char *) malloc(RTL_BUFFER_SIZE*sizeof(unsigned char));
	
	// Read in data
	tStart = (int) time(NULL);
	loopTimeOut = (int) duration;
	r = rtlsdr_read_async(dev, decoder_callback, (void *) bits, 0, RTL_BUFFER_SIZE);
	
	// Done
	if( do_exit ) {
		fprintf(stderr, "\nUser cancel, exiting...\n");
	} else {
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);
	}
	
	/*
	This looks like bad form but, for some reason, calling 'rtlsdr_close' 
	results in a segmentation fault for Python.  What's up with that?
	*/
	//rtlsdr_close(dev);
	
	// Cleanup
	free(raw);
	free(powerBuffer);

	// Return
	output = Py_BuildValue("O", bits);
	return output;
}

PyDoc_STRVAR(readRTL_doc, \
"Read in the data from a RTL SDR device and perform Manchester decoding, and\n\
return a list of bits (1 or 0) suitable for identifying Oregon Scientific v2.1\n\
and v3.0 sensor data.\n\
\n\
Inputs:\n\
  * duration - integer number of seconds to capture data for\n\
\n\
Outputs:\n\
 * bits - a list of ones and zeros for the data bits\n\
\n\
Based on:\n\
 * http://www.osengr.org/WxShield/Downloads/OregonScientific-RF-Protocols-II.pdf\n\
 * http://www.disk91.com/2013/technology/hardware/oregon-scientific-sensors-with-raspberry-pi/\n\
 ");


/*
  readRTLFile - Function for reading from a file created by 'rtl_sdr' and 
  returning a list of Manchester decoded bits.
*/

static PyObject *readRTLFile(PyObject *self, PyObject *args) {
	PyObject *output, *bits;
	int i;
	char *filename;
	struct sigaction sigact;

	if(!PyArg_ParseTuple(args, "s", &filename)) {
		PyErr_Format(PyExc_RuntimeError, "Invalid parameters");
		return NULL;
	}
	
	// Setup the signal handler	so that we can exit the callback function
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);
	
	// Ready the file
	FILE *fh = fopen(filename, "r");
	if(fh == NULL) {
		PyErr_Format(PyExc_IOError, "Cannot open file for reading");
		return NULL;
	}

	// Setup the output list
	bits = PyList_New(0);
	
	// Reset the loop control
	runningSum = 0;
	prevPower = 0;
	dataCounter = 0;
	prevEdge = -1;
	edgeCountDiff = -1;
	halfTime = 0;
	
	// Setup the variables - Power Detection
	powerBuffer = (float *) malloc(SMOOTH_WINDOW*sizeof(float));
	for(i=0; i<SMOOTH_WINDOW; i++) {
		*(powerBuffer + i) = 0.0;
	}
	
	// Setup the raw data buffer
	unsigned char *raw;
	raw = (unsigned char *) malloc(RTL_BUFFER_SIZE*sizeof(unsigned char));
	
	// Reset the loop control
	prevPower = 0;
	dataCounter = 0;
	prevEdge = -1;
	edgeCountDiff = -1;
	halfTime = 0;
	
	// Read in data and decode it
	loopTimeOut = 0;
	while( (i = fread(raw, sizeof(unsigned char), RTL_BUFFER_SIZE, fh)) > 0 ) {
		decoder_callback(raw, i, (void *) bits);
		
		//// Check for a request to exit
		if( do_exit ) {
			break;
		}
	}
	if( ferror(fh) ) {
		PyErr_Format(PyExc_IOError, "Error while reading from file");
		fclose(fh);
		free(raw);
		return NULL;
	}
	
	// Done
	fclose(fh);
	free(raw);

	// Return
	output = Py_BuildValue("O", bits);
	return output;
}

PyDoc_STRVAR(readRTLFile_doc, \
"Given an open file handle pointing to a RTL SDR recording, read in the data,\n\
perform Manchester decoding, and return a list of bits (1 or 0) suitable for\n\
identifying Oregon Scientific v2.1 and v3.0 sensor data.\n\
\n\
Inputs:\n\
  * filename - filename to open for reading\n\
\n\
Outputs:\n\
 * bits - a list of ones and zeros for the data bits\n\
\n\
Based on:\n\
 * http://www.osengr.org/WxShield/Downloads/OregonScientific-RF-Protocols-II.pdf\n\
 * http://www.disk91.com/2013/technology/hardware/oregon-scientific-sensors-with-raspberry-pi/\n\
");


/*
  Module Setup - Function Definitions and Documentation
*/

static PyMethodDef DecoderMethods[] = {
	{"readRTL", (PyCFunction) readRTL, METH_VARARGS, readRTL_doc}, 
	{"readRTLFile", (PyCFunction) readRTLFile, METH_VARARGS, readRTLFile_doc}, 
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(Decoder_doc, \
"Module to read in and Manchester decode Oregon Scientific v2.1 and v3.0 weather\n\
station data.");


/*
  Module Setup - Initialization
*/

PyMODINIT_FUNC initdecoder(void) {
	PyObject *m;

	// Module definitions and functions
	m = Py_InitModule3("decoder", DecoderMethods, Decoder_doc);
	
	// Version and revision information
	PyModule_AddObject(m, "__version__", PyString_FromString("0.1"));
}
