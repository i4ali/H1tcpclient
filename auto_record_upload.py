from h1tcpclient import H1
import argparse
import time
import threading
import custom_logger as cl
import logging
import json

retries = 20 # the number of times to poll H1 for response before timeout

def add_delay_before(secs):
    def real_add_delay(func_to_decorate):
        def wrapper(*args, **kwargs):
            time.sleep(secs)
            func_to_decorate(*args, **kwargs)
        return wrapper
    return real_add_delay


@add_delay_before(2)
def record_videos(camera, rec_length_secs, quantity):
    # loop for number of videos specified
    while quantity>0:
        # send record command for camera number
        start_record({"command":"record","camera":camera})
        # send stop command after time specified
        time.sleep(rec_length_secs)
        stop_record({"command":"stoprecord","camera":camera})
        quantity = quantity - 1
        time.sleep(5) # add estimated delay of 5seconds b/w videos

@add_delay_before(10)
def start_upload():
    resp = send_command_to_H1({"command":"upload", "icv":True})
    if resp == -1:
        raise IOError("Unable to start upload")
    elif resp == 0:
        log.info("upload start was successfull")

def connect(ip,port):
    h1.connect(ip,port)
    resp = send_command_to_H1({"command":"ping"})
    if resp == -1:
        raise IOError("Unable to connect to H1")
    elif resp == 0:
        log.info("connection to {0} on port {1} successful".format(ip,port))

def start_record(cmd):
    resp = send_command_to_H1(cmd)
    if resp == -1:
        log.warning("Unable to start recording on camera {0}".format(cmd['camera']))
    elif resp == 0:
        log.info("recording started on camera {0}".format(cmd['camera']))

def stop_record(cmd):
    resp = send_command_to_H1(cmd)
    if resp == -1:
        log.warning("Unable to stop recording on camera {0}".format(cmd['camera']))
    elif resp == 0:
        log.info("recording stopped on camera {0}".format(cmd['camera']))


def send_command_to_H1(cmd):
    h1.sendjson(cmd)  
    resp = get_response_from_H1(cmd)
    return resp


def get_response_from_H1(cmd):
    global retries
    while retries>0:
        h1.poll()
        response = h1.message
        if (response != ''):
            if (json.loads(response[4:])['status']) == 0 and (json.loads(response[4:])['command']) == cmd['command']:
                return 0
            elif (json.loads(response[4:])['status']) != 0 and (json.loads(response[4:])['command']) == cmd['command']:
                return -1
        retries = retries-1
        time.sleep(0.1)
    return -1 


if __name__ == '__main__':
    
    log = cl.custom_logger(logging.DEBUG)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ip", help="IP address of H1", type=str)
    parser.add_argument("-p", "--port", help="Port number (default: %(default)s)", type=int, default=9999)
    parser.add_argument("-c", "--camera", help="Camera number to record from (default: %(default)s)", type=int, action="store", 
        default=1)
    parser.add_argument("-t", "--recordingtime", help="Recording time in secs for video (default: %(default)s)", type=int, action="store",
        default=5)
    parser.add_argument("-v", "--videos", help="Number of videos to record (default: %(default)s)", type=int, action="store",
        default=1)
    parser.add_argument("-u", "--upload", help="Upload to Command Center(default: %(default)s)", action="store_true",
        default=False)

    args = parser.parse_args()

    h1 = H1()
    
    # connect to H1
    connect(args.ip,args.port)

    # record videos with camera number, length and number of videos specified
    record_videos(args.camera, args.recordingtime, args.videos)

    # upload if specified  
    if args.upload:
        start_upload()

    # close connection when done
    log.debug("disconnecting H1")
    h1.disconnect()

    

    

   
    
    
    
    