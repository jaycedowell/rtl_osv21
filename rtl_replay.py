#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Script to replay a saved 433MHz rtl_sdr file for testing purposes.
"""

import os
import re
import sys
import time
import numpy
import urllib
import struct
from datetime import datetime, timedelta

from _decode import readRTLFile
from rtl_osv21 import loadConfig, loadState, decodePacketv21


def main(args):
	# Read in the configuration file
	config = loadConfig()
	
	# Force verbose output
	config['verbose'] = True
	
	# Read in the rainfall state file
	prevRainDate, prevRainFall = loadState()

	# Find the bits in the freshly recorded data and remove the file
	fh = open(args[0], 'rb')
	bits = readRTLFile(fh)
	fh.close()
	
	# Find the packets and save the output
	i = 0
	wxData = {'dateutc': datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S")}
	while i < len(bits)-32:
		## Check for a valid preamble (and its logical negation counterpart)
		if sum(bits[i:i+32:2]) == 16 and sum(bits[i+1:i+1+32:2]) == 0:
			packet = bits[i::2]
			try:
				wxData, ps = decodePacketv21(packet, wxData, verbose=config['verbose'])
				i += 1
			except IndexError:
				i += 1
					
		else:
			i += 1
			
	# Report
	try:
		inside = "%.1f F with %i%% humidity (%s)" % (wxData['indoortempf'], wxData['indoorhumidity'], wxData['comfort'])
		print "Inside Conditions:"
		print " "+inside
	except KeyError, e:
		pass
	try:
		outside = "%.1f F with %i%% humidity (dew point %.1f F)" % (wxData['tempf'], wxData['humidity'], wxData['dewptf'])
		print "Outside Conditions:"
		print " "+outside
	except KeyError, e:
		pass
	try:
		if prevRainFall is not None:
			rain = "%.2f in since local midnight" % (wxData['dailyrainin']-prevRainFall,)
		else:
			rain = "%.2f in since last reset" % wxData['dailyrainin']
		print "Rain:"
		print " "+rain
	except KeyError, e:
		pass
	try:
		wind = "Average %.1f mph with gusts of %.1f mph from %i degrees" % (wxData['windspeedmph'], wxData['windgustmph'], wxData['winddir'])
		print "Wind:"
		print " "+wind
	except KeyError, e:
		pass
	try:
		forecast = "%s (%.2f in-Hg)" % (wxData['forecast'], wxData['baromin'])
		print "Forecast:"
		print " "+forecast
	except KeyError, e:
		pass


if __name__ == "__main__":
	main(sys.argv[1:])