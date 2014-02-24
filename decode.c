#include "Python.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* The version 2.1 and 3.0 sensors use a bit rate of 1,024 Hz which is
   approximately 488 samples @ 1Ms/s.
*/
#define SMOOTH_WINDOW 488


static PyObject *readRTLFile(PyObject *self, PyObject *args) {
	PyObject *ph, *output, *bits, *temp;
	int i, j;
	
	if(!PyArg_ParseTuple(args, "O", &ph)) {
		PyErr_Format(PyExc_RuntimeError, "Invalid parameters");
		return NULL;
	}
	
	// Ready the file
	FILE *fh = PyFile_AsFile(ph);
	PyFile_IncUseCount((PyFileObject *) ph);
	
	// Setup the output list
	bits = PyList_New(0);
	
	// Setup the variables
	float threshold = 6800.0;
	float real, imag;
	float instPower;
	float runningSum = 0;
	float *buffer;
	buffer = (float *) malloc(SMOOTH_WINDOW*sizeof(float));
	for(i=0; i<SMOOTH_WINDOW; i++) {
		*(buffer + i) = 0.0;
	}
	
	// Setup the control loop
	int power, edge;
	int prevPower = 0;
	long dataCounter = 0;
	long prevEdge = -1;
	long edgeCountDiff = -1;
	long halfTime = 0;
	int addBit;
	
	// Go!
	unsigned char raw[2*SMOOTH_WINDOW];
	i = fread(raw, 1, sizeof(raw), fh);
	while( !feof(fh) ) {
		for(j=0; j<SMOOTH_WINDOW; j++) {
			// I/Q samples to power
			real = ((float) raw[2*j+0]) - 127.0;
			imag = ((float) raw[2*j+1]) - 127.0;
			instPower = real*real + imag*imag;
			dataCounter += 1;
		
			// Moving average
			runningSum += instPower - *(buffer + j);
			*(buffer + j) = instPower;
		
			// Convert to an integer
			if( runningSum >= threshold*SMOOTH_WINDOW ) {
				power = 1;
			} else {
				power = 0;
			}
		
			// Edge detection
			edge = power - prevPower;
			prevPower = power;
		
			/* Timing
			   NOTE:  Some of these values may need to be tweaked for v3.0
			          sensors.  See: 
			          http://www.osengr.org/WxShield/Downloads/OregonScientific-RF-Protocols-II.pdf
			*/
			if( edge != 0 ) {
				if( prevEdge < 0 ) {
					prevEdge = dataCounter;
				}
				edgeCountDiff = dataCounter - prevEdge;
			}
				
			if( edge == 1 ) {
				// Rising edge
		
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
					PyList_Append(bits, temp);
					Py_DECREF(temp);
				}
			
			} else if( edge == -1 ) {
				// Falling edge
			
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
					PyList_Append(bits, temp);
					Py_DECREF(temp);
				}
			}
		}
		
		// Read in the next set of samples
		i = fread(raw, 1, sizeof(raw), fh);
	}
	
	// Done
	free(buffer);
	PyFile_DecUseCount((PyFileObject *) ph);
	
	// Return
	output = Py_BuildValue("O", bits);
	return output;
}

PyDoc_STRVAR(readRTLFile_doc, \
"Given an open file handle pointing to a RTL SDR recording, read in the data,\n\
perform Manchester decoding, and return a list of bits (1 or 0) suitable for\n\
identifying Oregon Scientific v2.1 and v3.0 sensor data.\n\
\n\
Based on:\n\
 * http://www.osengr.org/WxShield/Downloads/OregonScientific-RF-Protocols-II.pdf\n\
 * http://www.disk91.com/2013/technology/hardware/oregon-scientific-sensors-with-raspberry-pi/\n\
 ");


/*
  Module Setup - Function Definitions and Documentation
*/

static PyMethodDef DecodeMethods[] = {
	{"readRTLFile", (PyCFunction) readRTLFile, METH_VARARGS, readRTLFile_doc}, 
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(Decode_doc, \
"Module to read in and Manchester decode Oregon Scientific v2.1 and v3.0 weather\n\
station data.");


/*
  Module Setup - Initialization
*/

PyMODINIT_FUNC init_decode(void) {
	PyObject *m;

	// Module definitions and functions
	m = Py_InitModule3("_decode", DecodeMethods, Decode_doc);
	
	// Version and revision information
	PyModule_AddObject(m, "__version__", PyString_FromString("0.1"));
	PyModule_AddObject(m, "__revision__", PyString_FromString("$Rev$"));
}
