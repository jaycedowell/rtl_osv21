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

from decoder import readRTLFile
from parser import parsePacketv21
from utils import loadConfig, length_mm2in, temp_C2F, pressure_mb2inHg, speed_ms2mph
from utils import computeDewPoint, computeWindchill, computeSeaLevelPressure
from rtl_osv21 import CONFIG_FILE


def main(args):
	# Read in the configuration file
	config = loadConfig(CONFIG_FILE)
	
	# Find the bits in the freshly recorded data and remove the file
	fh = open(args[0], 'rb')
	bits = readRTLFile(fh)
	fh.close()
	
	# Find the packets and save the output
	i = 0
	output = {}
	while i < len(bits)-32:
		## Check for a valid preamble (and its logical negation counterpart)
		if sum(bits[i:i+32:2]) == 16 and sum(bits[i+1:i+1+32:2]) == 0:
			packet1 = bits[i+0::2]
			packet2 = bits[i+1::2]
			
			try:
				valid, sensorName, channel, sensorData = parsePacketv21(packet1, verbose=True)
				if not valid:
					valid, sensorName, channel, sensorData = parsePacketv21(packet2, verbose=True)
					
				if valid:
					if sensorName in ('BHTR968', 'THGR268', 'THGR968'):
						sensorData['dewpoint'] = computeDewPoint(sensorData['temperature'], sensorData['humidity'])
					if sensorName in ('BHTR968',):
						sensorData['pressure'] = computeSeaLevelPressure(sensorData['pressure'], config['elevation'])
					if sensorName == 'BHTR968':
						sensorData['indoorTemperature'] = sensorData['temperature']
						del sensorData['temperature']
						sensorData['indoorHumidity'] = sensorData['humidity']
						del sensorData['humidity']
						sensorData['indoorDewpoint'] = sensorData['dewpoint']
						del sensorData['dewpoint']
						
					for key in sensorData.keys():
						if key in ('temperature', 'humidity', 'dewpoint'):
							if sensorName == 'THGR968':
								output[key] = sensorData[key]
							else:
								try:
									output['alt%s' % key.capitalize()][channel-1] = sensorData[key]
								except KeyError:
									output['alt%s' % key.capitalize()] = [None, None, None, None]
									output['alt%s' % key.capitalize()][channel-1] = sensorData[key]
						else:
							output[key] = sensorData[key]
							
			except IndexError:
				pass
		i += 1
		
	# Compute combined quantities
	if 'temperature' in output.keys() and 'average' in output.keys():
		output['windchill'] = computeWindchill(output['temperature'], output['average'])
		
	# Report
	if 'indoorTemperature' in output.keys():
		print "Indoor Conditions:"
		print " -> %.1f F with %i%% humidity (%s)" % (temp_C2F(output['indoorTemperature']), output['indoorHumidity'], output['comfortLevel'])
		print " -> dew point is %.1f F" % (temp_C2F(output['indoorDewpoint']),)
		print " -> barometric pressure is %.2f in-Hg" % pressure_mb2inHg(output['pressure'])
		print " "
	
	if 'temperature' in output.keys():
		print "Outdoor Conditions:"
		print " -> %.1f F with %i%% humidity" % (temp_C2F(output['temperature']), output['humidity'])
		print " -> dew point is %.1f F" % (temp_C2F(output['dewpoint']),)
		if 'windchill' in output.keys():
			print " -> windchill is %.1f F" % (temp_C2F(output['windchill']),)
		if 'altTemperature' in output.keys():
			for i in xrange(4):
				if output['altTemperature'][i] is not None:
					t, h, d = output['altTemperature'][i], output['altHumidity'][i], output['altDewpoint'][i]
					print "    #%i: %.1f F with %i%% humidity" % (i+1, temp_C2F(t), h)
					print "         dew point is %.1f F" % (temp_C2F(d),)
		print " "
		
	if 'rainrate' in output.keys():
		print "Rainfall:"
		print " -> %.2f in/hr, %.2f in total" % (length_mm2in(output['rainrate']), length_mm2in(output['rainfall']))
		print " "
		
	if 'average' in output.keys():
		print "Wind:"
		print "-> average %.1f mph @ %i degrees" % (speed_ms2mph(output['average']), output['direction'])
		print "-> gust %.1f mph" % speed_ms2mph(output['gust'])
		print " "
	
	if 'forecast' in output.keys():
		print "Forecast:"
		print " -> %s" % output['forecast']
		print " "


if __name__ == "__main__":
	main(sys.argv[1:])