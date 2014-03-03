#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Script to capture a rtl_sdr file for testing purposes.

This script takes two arguments:
 1) the duration of the recording in seconds
 2) a filename for where to write the data to

The resulting data can be read in with NumPy via:
>>> import numpy
>>> data = numpy.fromfile(filename, dtype=numpy.uint8)
>>> dataR = data[0::2].astype(numpy.float32) - 127
>>> dataI = data[1::2].astype(numpy.float32) - 127
>>> data = dataR + 1j*dataI
"""

import os
import sys

from config import CONFIG_FILE, loadConfig
from recorder import record433MHzData


def main(args):
	# Parse the command line
	if len(args) != 2:
		raise RuntimeError("Invalid number of arguments provided, expected a duration and a filename")
	duration = float(args[0])
	filename = args[1]

	# Read in the configuration file
	config = loadConfig(CONFIG_FILE)
	
	# Make sure that we can safely write to the file
	if os.path.exists(filename):
		goAhead = raw_input('File already exists, overwrite? [y/n]')
		if goAhead in ('n', 'N', ''):
			sys.exit()
			
	# Record the data
	record433MHzData(filename, duration, rtlsdrPath=config['rtlsdr'], useTimeout=config['useTimeout'])
	
	# Report
	print "Recorded %i bytes to '%s'" % (os.path.getsize(filename), filename)


if __name__ == "__main__":
	main(sys.argv[1:])