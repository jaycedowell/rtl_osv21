# -*- coding: utf-8 -*-

"""
Module for recording RTL SDR data for later analysis.
"""

import os
import re

__version__ = "0.1"
__all__ = ['record433MHzData', '__version__', '__all__']


def _getParameters():
	"""
	Get the frequency and sample rate parameters from the decoder.c file
	and return them as a two element tuple of frequency in Hz and sample
	rate in Hz.
	
	If the decoder.c file cannot be found, the default values of 433800000
	for the frequency and 100000 for the sample rate are returned.
	"""
	
	# Find the file
	decoderFilename = os.path.dirname(os.path.abspath(__file__))
	decoderFilename = os.path.join(decoderFilename, "decoder.c")
	
	# Build the RegEx for find the '#define' statements
	defRE = re.compile(r'^#define\s+(?P<name>.*?)\s+(?P<value>.*)$')
	
	freq = 433800000
	srate = 1000000
	
	try:
		# Parse
		fh = open(decoderFilename, 'r')
		for line in fh:
			line = line.replace('\n', '')
			mtch = defRE.match(line)
			
			## Do we have a '#define' statement?
			if mtch is not None:
				name = mtch.group('name')
				value = mtch.group('value')
			
				if name == 'FREQUENCY':
					freq = int(value)
				elif name == 'SAMPLE_RATE':
					srate = int(value)
				else:
					pass
					
	except IOError:
		pass
		
	fh.close()
	
	return freq, srate


# Load in the frequency and sample rate to use
_rtlsdrFreq, _rtlsdrRate = _getParameters()	


def record433MHzData(filename, duration, rtlsdrPath=None, useTimeout=False):
	"""
	Call the "rtl_sdr" program to record data at 433.8 MHz for the specified 
	duration in second to the specified filename.  
	
	Keywords accepted are:
	  * 'rtlsdrPath' to specify the full path of the executable and 
	  * 'useTimeout' for whether or not to wrap the "rtl_sdr" call with 
	    "timeout".  This feature is useful on some systems, such as the 
	    Raspberry Pi, where the "rtl_sdr" hangs after recording data.
	"""
	
	# Setup the duration in samples
	samplesToRecord = int(duration*_rtlsdrRate)
	
	# Setup the program
	if rtlsdrPath is None:
		cmd = "rtl_sdr"
	else:
		cmd = rtlsdrPath
	cmd = "%s -f %i -s %i -n %i %s" % (cmd, _rtlsdrFreq, _rtlsdrRate, samplesToRecord, filename)
	if useTimeout:
		timeoutPeriod = duration + 10
		cmd = "timeout -s 9 %i %s" % (timeoutPeriod, cmd)
		
	# Call
	os.system(cmd)
	
	# Done
	return True