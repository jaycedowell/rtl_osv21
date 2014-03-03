"""
Module for interfacing with the sqlite3 database.
"""

import os
import time
import sqlite3

__version__ = "0.1"
__all__ = ["Archive", "__version__", "__all__"]


class Archive(object):
	_dbConn = None
	_cursor = None
	_dbMapper = {'temperature': 'outTemp', 
				 'humidity': 'outHumidity', 
				 'dewpoint': 'outDewpoint', 
				 'windchill': 'windchill', 
				 'indoorTemperature': 'inTemp', 
				 'indoorHumidity': 'inHumidity',
				 'indoorDewpoint': 'inDewpoint', 
				 'pressure': 'barometer',
				 'average': 'windSpeed', 
				 'gust': 'windGust', 
				 'direction': 'windDir',
				 'rainrate': 'rainRate', 
				 'rainfall': 'rain',
				 'uvIndex': 'uv'}
				 
	def __init__(self):
		self._dbName = os.path.join(os.path.dirname(__file__), 'archive', 'wx-data.db')
		if not os.path.exists(self._dbName):
			raise RuntimeError("Archive database not found")
			
		self.open()
		
	def dict_factory(self, cursor, row):
		d = {}
		for idx, col in enumerate(cursor.description):
			d[col[0]] = row[idx]
		return d
    	
	def open(self):
		"""
		Open the database.
		"""
		
		self._dbConn = sqlite3.connect(self._dbName)
		self._dbConn.row_factory = self.dict_factory
		self._cursor = self._dbConn.cursor()
		
	def close(self):
		"""
		Close the database.
		"""
	
		if self._dbConn is not None:
			self._dbConn.commit()
			self._dbConn.close()
		
	def getData(self, age=0):
		"""
		Return a collection of data a certain number of seconds into the past.
		"""
	
		if self._dbConn is None:
			self.open()
		
		# Fetch the entries that match
		if age <= 0:
			self._cursor.execute('SELECT * FROM wx ORDER BY dateTime DESC')
		else:
			# Figure out how far to look back into the database
			tNow = time.time()
			tLookback = tNow - age
			self._cursor.execute('SELECT * FROM wx WHERE dateTime >= %i ORDER BY dateTime' % tLookback)
		row = self._cursor.fetchone()

		# Check for an empty database
		if row is None:
			return 0, {}
			
		# Convert it to the "standard" dictionary format
		timestamp = row['dateTime']
		output = {'temperature': row['outTemp'], 'humidity': row['outHumidity'], 
		          'dewpoint': row['outDewpoint'], 'windchill': row['windchill'], 
		          'indoorTemperature': row['inTemp'], 'indoorHumidity': row['inHumidity'], 
		          'indoorDewpoint': row['inDewpoint'], 'pressure': row['barometer'], 
		          'rainrate': row['rainRate'], 'rainfall': row['rain'], 
		          'altTemperature': [], 'altHumdity': [], 'altDewpoint': [],
			  'uvIndex': row['uv']}
		for i in xrange(1, 5):
			output['altTemperature'].append( row['outTemp%i' % i] if row['outTemp%i' % i] != -99 else None )
			output['altHumdity'].append( row['outHumidity%i' % i] if row['outHumidity%i' % i] != -99 else None )
			output['altDewpoint'].append( row['outDewpoint%i' % i] if row['outDewpoint%i' % i] != -99 else None )
	
		return timestamp, output

	def writeData(self, timestamp, data):
		"""
		Write a collection of data to the database.
		"""
		if self._dbConn is None:
			self.open()
		
		# Build up the values to insert
		cNames = ['dateTime', 'usUnits']
		dValues = [int(timestamp), 0]
		for key in data.keys():
			try:
				cNames.append( self._dbMapper[key] )
				dValues.append( data[key] )
			except KeyError:
				if key[:3] == 'alt':
					if key[3:6] == 'Tem':
						nameBase = 'outTemp'
					elif key[3:6] == 'Hum':
						nameBase = 'outHumidity'
					else:
						nameBase = 'outDewpoint'
						
					for i in xrange(len(data[key])):
						if data[key][i] is not None:
							cNames.append( "%s%i" % (nameBase, i+1) )
							dValues.append( data[key][i] )
							
		self._cursor.execute('INSERT INTO wx (%s) VALUES (%s)' % (','.join(cNames), ','.join([str(v) for v in dValues])))
		self._dbConn.commit()
	
		return True
