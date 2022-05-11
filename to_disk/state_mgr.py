#!/usr/bin/python3

import os
import sys
import subprocess
import glob
import time
import random
import shutil
import multiprocessing
import threading
import asyncio.subprocess
import argparse


conn_states = {} ### key is start state 

#### coverage
cov_tran = set()
cov_state = set()
coveredall = set() ### already covered all for each state


### channel for trans
ch = {} ### e.g., (cur, next): "ch3"
#### trans details
c1_tran = {} 
c2_tran = {}
c3_tran = {}
c4_tran = {}

gadget = {"hid":"001"} 


interval = 1
get_next = 0
unexplored = False
retry = False

#### current state
state = 0

ROOT = os.path.dirname(os.path.abspath(__file__))

def _execute(cmds):
    error = False
    try:
        output = subprocess.check_output(cmds, stderr=subprocess.STDOUT)
    except Exception as e:
        output = str(e)
        error = True
    return output, error

def get_dev_num(bus):
    d = os.listdir("/dev/bus/usb/" + bus)
    for f in d:
        if f.find("001") < 0:
            return f
    return ""

def c4_dispatch(arg):

    if arg.startswith("ifconfig"):
        try:
            #### 1. read usb if interface
            ifname = open(str(arg.split(',')[1]), 'r').read()

            #### 2. issue cmd "ifconfig" 
            cmds = ["ifconfig", ifname.strip(), str(arg.split(',')[2])]
            output, error = _execute(cmds)
        except Exception as e:
            output = str(e)
        return

    #### for serial
    if arg.find("port_num") > 0 and (arg.startswith("read") or arg.startswith("write")) :
        try:
            #### 1. read port_num i.e., /dev/ttyGSX
            port_num = open(str(arg.split(',')[1]), 'r').read()

            #### 2. issue cmd 
            cmds = ["/root/c4_fz", "-s", str(arg.split(',')[0]), "-a", "/dev/ttyGS" + str(port_num.strip()), "-b", str(arg.split(',')[2])]
            output, error = _execute(cmds)
        except Exception as e:
            output = str(e)
        return

    cmds = ["/root/c4_fz", "-s", str(arg.split(',')[0]), "-a", str(arg.split(',')[1]), "-b", str(arg.split(',')[2])]
    output, error = _execute(cmds)


def c3_dispatch(arg, bus):
    arg1 = arg.split(',')[0]
    if arg1 == "int" : 
        types = 1
    elif arg1 == "bulk" : 
        types = 2
    elif arg1 == "iso" : 
        types = 3
    else: 
        types = 4
        
    arg2 = arg.split(',')[1]
    if arg2 == "out" : 
        dir_out = 1
    else:
        dir_out = 0

    #### iteration
    iterate = arg.split(',')[2]

    #### length
    length = arg.split(',')[3]

    arg5 = arg.split(',')[4]
    if arg5.startswith("hex:") : 
        arg5 = arg5[len("hex:0x"):]

    ### get device number
    dev_num = get_dev_num(bus)

    if len(arg5) == 0:
        cmds = ["/root/c3_fz", "-D", "/dev/bus/usb/" + bus + "/"+ dev_num, "-t", str(types), "-d", str(dir_out), "-i", iterate, "-l", length, "-a", '']
    else:
        cmds = ["/root/c3_fz", "-D", "/dev/bus/usb/" + bus + "/"+ dev_num, "-t", str(types), "-d", str(dir_out), "-i", iterate, "-l", length, "-a", str(arg5)]
    output, error = _execute(cmds)


def c2_dispatch(arg, bus, intf, alt):

    ### get device number
    dev_num = get_dev_num(bus)

    cmds = ["/root/c2_fz", "-D", "/dev/bus/usb/" + bus + "/" + dev_num, "-c", str(arg), "-a", str(intf), "-b", str(alt)]
    output, error = _execute(cmds)


def c1_dispatch(arg):
    global state

    trans = arg.split(',')[0]
    g_dir = arg.split(',')[1]
    arg0 = ''
    arg1 = ''
    arg2 = ''

    try:
        if trans == "01":
            arg0 = arg.split(',')[2]
            arg1 = arg.split(',')[3]
            if len(arg.split(',')[4]) != 0:
                arg2 = arg.split(',')[4]
        elif trans == "23" or trans == "34" or trans == "45" or trans == "43" or trans == "32":
            arg0 = arg.split(',')[2]
        elif trans == "55":     ### c1 generic mutation at the cur state
            arg0 = arg.split(',')[2]

        cmds = ["/root/c1_fz", "-t", str(trans), "-d", str(g_dir), "-a", str(arg0), "-b", str(arg1), "-c", str(arg2)]
        output, error = _execute(cmds)

        if trans == "01":
            state = 1
        elif trans == "12":
            state = 2
        elif trans == "23":
            state = 3
        elif trans == "34":
            state = 4
        elif trans == "45":
            state = 5
        elif trans == "54":
            state = 4
        elif trans == "43":
            state = 3
        elif trans == "32":
            state = 2
        elif trans == "21":
            state = 1
        elif trans == "10":
            state = 0

    except Exception as e:
        output = str(e)


def c1_mutate ():

    while True:

        #### clean existing one
        flag = random.randint(1,10) % 2
        if flag == 1:
            c1_dispatch(c1_tran[(5, 4)])
            trans_interval()
        c1_dispatch(c1_tran[(4, 3)])
        trans_interval()
        c1_dispatch(c1_tran[(3, 2)])
        trans_interval()
        c1_dispatch(c1_tran[(2, 1)])
        trans_interval()
        c1_dispatch(c1_tran[(1, 0)])
        trans_interval()

        c1_dispatch(c1_tran[(0, 1)])
        trans_interval()
        c1_dispatch(c1_tran[(1, 2)])
        trans_interval()
        c1_dispatch(c1_tran[(2, 3)])
        trans_interval()
        c1_dispatch(c1_tran[(3, 4)])
        trans_interval()
        c1_dispatch(c1_tran[(4, 5)])

        time.sleep(60*3) 


def c2_mutate (gadget, bus):
    global state
    g_path = "/sys/kernel/config/usb_gadget/"
    
    loop_count = 1;
    while True:
        if state >= 5:
            time.sleep(2)

        loop_count += 1;


def load_fsm(fsm_file):

    with open(os.path.join("/root/", fsm_file), 'r') as lines:
        for l in lines:
            if l.startswith("t:") :  
                states = l[len("t:"):]
                c_state = int(states.split(',')[0])
                n_state = int(states.split(',')[1])

                if not c_state in conn_states.keys():
                    conn_states[c_state] = []
                if not (c_state == 5 and n_state <= 5):
                    conn_states[c_state].insert(0, n_state)
            elif l.startswith("c1:"):  
                c1 = l[len("c1:"):]
                c1_tran[(c_state, n_state)] = c1.strip()  ### tuple packing
                ch[(c_state, n_state)] = "c1"
            elif l.startswith("c2:"):
                c2 = l[len("c2:"):]
                c2_tran[(c_state, n_state)] = c2.strip()
                ch[(c_state, n_state)] = "c2"
            elif l.startswith("c3:"):
                c3 = l[len("c3:"):]
                c3_tran[(c_state, n_state)] = c3.strip()
                ch[(c_state, n_state)] = "c3"
            elif l.startswith("c4:"):
                c4 = l[len("c4:"):]
                c4_tran[(c_state, n_state)] = c4.strip()
                ch[(c_state, n_state)] = "c4"

def load_rule(rule_file):
    global get_next, interval, unexplored, retry

    with open(os.path.join("/root/", rule_file), 'r') as lines:
        for l in lines:
            if l.startswith("1:") :   ### interval
                interval = int(l[len("1:"):])
                print ("rule1: ", interval)
            if l.startswith("2:") :   ### cov track
                if l[len("2:"):].strip() == 'trans':
                    get_next = get_next_tran
                    print ("rule2: trans track")
                else:
                    get_next = get_next_state
                    print ("rule2: state track")
            if l.startswith("3:") :  
                if l[len("3:"):].strip() == 'unexplored':
                    unexplored = True
                    print ("rule3: unexplored")
                else: 
                    print ("rule3: random")
            if l.startswith("4:") :  
                if l[len("4:"):].strip() == "retry":
                    retry = True
                    print ("rule4: retry")
                else: 
                    print ("rule4: non-retry")


def loop(gadget, bus):
    global state, get_next

    threading.Thread(name="c1_mutate", target=c1_mutate, ).start()
    threading.Thread(name="c2_mutate", target=c2_mutate, args=(gadget, bus,)).start()

    while True:
        if state >= 5:
            time.sleep(2)

            if bus == "001" :
                state = run_trans(get_next(state), bus)
                trans_interval()

def run_trans(tran, bus):
    if tran in ch:
        if ch[tran] == "c3":
            multiprocessing.Process(name="c3_dispatch", target=c3_dispatch, 
                args=(c3_tran[tran], bus,)).start()
        elif ch[tran] == "c4":
            multiprocessing.Process(name="c4_dispatch", target=c4_dispatch, 
                args=(c4_tran[tran],)).start() ### because read can be blocked
    time.sleep(1) #### EDIT
    return tran[1]

def get_next_tran(cur):

    #### 1. one and only trans
    if len(conn_states[cur]) == 1:
        cov_tran.add ((cur, conn_states[cur][0]))
        return (cur, conn_states[cur][0])

    #### 2. unexplored trans first
    if (unexplored == True) and (not cur in coveredall):
        for next_S in conn_states[cur]:
            if not (cur, next_S) in cov_tran:
                cov_tran.add ((cur, next_S)) 
                return (cur, next_S)
        coveredall.add (cur) 

    #### 3. random select
    next_S = conn_states[cur][random.randint(1, len(conn_states[cur]))-1]
    return (cur, next_S)


def get_next_state(cur):
    #### 1. one and only next state
    if len(conn_states[cur]) == 1:
        cov_state.add (conn_states[cur][0]) 
        return (cur, conn_states[cur][0])
        
    #### 2. unexplored state first
    if (unexplored == True) and (not cur in coveredall):
        for next_S in conn_states[cur]:
            if not next_S in cov_state:
                cov_state.add (next_S) 
                return (cur, next_S)
        coveredall.add (cur) 

    #### 3. random select
    next_S = conn_states[cur][random.randint(1, len(conn_states[cur]))-1]
    return (cur, next_S)

def trans_interval():
    global interval
    time.sleep(interval)


if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument('fsm', type=str, help='specify target gadget name')
    args = parser.parse_args()

    load_rule("rule.txt")
    load_fsm("fsm_" + args.fsm + ".txt")

    #### main loop 
    loop(args.fsm, gadget[args.fsm]) 

