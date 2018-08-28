from h1tcpclient import H1
import argparse
import time


def add_delay_before(secs):
    def real_add_delay(func_to_decorate):
        def wrapper(*args, **kwargs):
            time.sleep(secs)
            func_to_decorate(*args, **kwargs)
        return wrapper
    return real_add_delay


def connect(ip,port):
    h1.connect(ip, port)

def record_videos(camera, rec_length_secs, quantity):
    # loop for number of videos specified
    while quantity>0:
        # send record command for camera number
        h1.sendjson({"command":"record","camera":camera})

        # send stop command after time specified
        time.sleep(rec_length_secs)
        h1.sendjson({"command":"stoprecord","camera":camera})
        quantity = quantity - 1
        time.sleep(5) # add estimated delay of 5seconds b/w videos

@add_delay_before(10)
def upload_to_CC():
    h1.sendjson({"command":"upload", "icv":True})


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ip", help="IP address of H1", type=str)
    parser.add_argument("-p", "--port", help="Port number (default: %(default)s)", type=int, default=9999)
    parser.add_argument("-c", "--camera", help="Camera number to record from (default: %(default)s)", type=int, action="store", 
        default=1)
    parser.add_argument("-t", "--recordingtime", help="Recording time in secs for video (default: %(default)s)", type=int, action="store",
        default=5)
    parser.add_argument("-v", "--videos", help="Number of videos to record (default: %(default)s)", type=int, action="store",
        default=1)
    parser.add_argument("-u", "--upload", help="Upload to CC (default: %(default)s)", action="store_true",
        default=False)

    args = parser.parse_args()

    # instantiate connection to H1
    h1 = H1()
    connect(args.ip, args.port)

    # record videos with camera, length and number of videos specified
    record_videos(args.camera, args.recordingtime, args.videos)

    # upload if specified  
    if args.upload:
        upload_to_CC()
    

   
    
    
    
    