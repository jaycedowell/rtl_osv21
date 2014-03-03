#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Script to replay a saved 433MHz rtl_sdr file for testing purposes.

This script takes one argument:
 1) a filename to read raw RTL SDR data from
"""

import sys

from config import CONFIG_FILE, loadConfig
from decoder import readRTLFile
from parser import parseBitStream
from utils import generateWeatherReport


def main(args):
	# Validate arguments
	if len(args) != 1:
		raise RuntimeError("Invalid number of arguments provided, expected a filename")
	filename = args[0]
	
	# Read in the configuration file
	config = loadConfig(CONFIG_FILE)
	
	# Find the bits in the freshly recorded data and remove the file
	bits = readRTLFile(filename)
	
	# Find the packets
	output = parseBitStream(bits, elevation=config['elevation'], verbose=True)
	
	# Report
	print " "
	print generateWeatherReport(output)


if __name__ == "__main__":
	main(sys.argv[1:])