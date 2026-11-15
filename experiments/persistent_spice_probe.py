"""Isolated proof: resume RC and DFF after alter, without starting a new analysis.
Run: python experiments/persistent_spice_probe.py [path to libngspice-0.dll]
Uses only the Python standard library. It does not change saved workbenches.
"""
import ctypes as ct
import os
from pathlib import Path
import sys

class Vector(ct.Structure):
    _fields_ = [("name", ct.c_char_p), ("type", ct.c_int), ("flags", ct.c_short),
                ("real", ct.POINTER(ct.c_double)), ("complex", ct.c_void_p), ("length", ct.c_int)]

runtime = Path(sys.argv[1] if len(sys.argv) > 1 else
               "build/spice-local/ucrt64/bin/libngspice-0.dll").resolve()
dll_directory = os.add_dll_directory(str(runtime.parent))
lib = ct.CDLL(str(runtime))
log = []
@ct.CFUNCTYPE(ct.c_int, ct.c_char_p, ct.c_int, ct.c_void_p)
def output(message, _id, _user):
    log.append(message.decode(errors="replace"))
    return 0
@ct.CFUNCTYPE(ct.c_int, ct.c_int, ct.c_bool, ct.c_bool, ct.c_int, ct.c_void_p)
def controlled_exit(status, _unload, _quit, _id, _user):
    log.append(f"controlled exit {status}")
    return 0
lib.ngSpice_Init.argtypes = [ct.c_void_p] * 7
lib.ngSpice_Init(output, None, controlled_exit, None, None, None, None)
lib.ngSpice_Command.argtypes = [ct.c_char_p]
lib.ngSpice_Circ.argtypes = [ct.POINTER(ct.c_char_p)]
lib.ngGet_Vec_Info.argtypes = [ct.c_char_p]
lib.ngGet_Vec_Info.restype = ct.POINTER(Vector)

def command(text):
    before = len(log)
    result = lib.ngSpice_Command(text.encode())
    errors = [line for line in log[before:] if "error" in line.lower() or "fatal" in line.lower()]
    if result or errors:
        raise RuntimeError((text, result, errors))

def circuit(lines):
    encoded = [line.encode() for line in lines]
    array = (ct.c_char_p * (len(encoded) + 1))(*encoded, None)
    result = lib.ngSpice_Circ(array)
    if result:
        raise RuntimeError((result, log[-10:]))

def value(name):
    vector = lib.ngGet_Vec_Info(name.encode())
    if not vector or vector.contents.length < 1:
        raise RuntimeError((name, log[-10:]))
    return vector.contents.real[vector.contents.length - 1]

def advance(until, first=False):
    command("delete all")
    command(f"stop when time >= {until:.17g}")
    command("tran 1e-5 1 uic" if first else "resume")

circuit(["persistent RC", "Vbattery supply 0 5", "Rfeed supply out 1k", "Rload out 0 1k",
         "Cstore out 0 1u IC=0", ".end"])
advance(.005, first=True)
charged = value("out")
assert charged > 2.49, charged
command("alter Rfeed = 1e12")
advance(.006)
discharged = value("out")
assert .85 < discharged < 1, discharged
print(f"RC preserved: {charged:.6f} V -> {discharged:.6f} V after disconnecting feed")
command("destroy all")
models = runtime.parent.parent / "lib" / "ngspice" / "digital.cm"
command(f"codemodel {models.as_posix()}")
circuit(["persistent DFF", "Vdata data 0 0", "Vclock clock 0 0",
         "Ainput [data clock] [d clk] adc", "Aff d clk null null qd null ff",
         "Aout [qd] [q] dac", "Rload q 0 10k",
         ".model adc adc_bridge(in_low=1.5 in_high=3.5)",
         ".model dac dac_bridge(out_low=0 out_high=5)",
         ".model ff d_dff(ic=0 rise_delay=1n fall_delay=1n)", ".end"])
advance(.001, first=True)
command("alter Vdata = 5")
advance(.002)
assert value("q") < .1
command("alter Vclock = 5")
advance(.003)
assert value("q") > 4.9
command("alter Vdata = 0")
advance(.004)
assert value("q") > 4.9, "DFF lost memory while clock remained high"
command("alter Vclock = 0")
advance(.005)
assert value("q") > 4.9
command("alter Vclock = 5")
advance(.006)
assert value("q") < .1
print("DFF preserved: Q changes only on the rising edge, despite live data changes")
