#! /usr/bin/python

# Simple CLI client for the corba module of vlc. Depends on pyorbit.
# Best used with IPython (completion, ...)

import sys
import ORBit, CORBA

def quit ():
	try:
		mc.exit()
	except:
		pass
		
print "IDL loading"
ORBit.load_typelib ("./MediaControl.so")
import VLC

if len(sys.argv) < 1:
	print "Usage: %s" % sys.argv[0]
	sys.exit(1)

print "ORB initialization"
orb = CORBA.ORB_init()

ior = open("/tmp/vlc-ior.ref").readline()
mc = orb.string_to_object(ior)

print "Object mc %s" % mc

pos = mc.get_media_position (0,0)
print "pos = mc.get_media_position (0,0)"
print pos

