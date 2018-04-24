from distutils.core import setup, Extension

mcs_module = Extension('mcsDbg._mcs',
		       sources=['mcsDbg/mcs.c', 'mcsDbg/follow.c'])

setup (name = 'mcsDbg',
       description = 'Debugging tools for MCS algorithms',
       py_modules=['mcsDbg.mcs'],
       ext_modules = [mcs_module])
