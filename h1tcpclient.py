#!/usr/bin/python

import socket

import os
import errno
import struct
import binascii
import json
import time


try:
	import tkinter as tk
except:
	import Tkinter as tk

root = tk.Tk()

def spawnprocess(args):
	if os.fork() == 0:
		os.execvp(args[0],args);

class H1:
	def __init__(self):
		self.socket = None
		root.after(100,self.poll)
		self.buffer = b''
		self.host = "192.168.0.2"

	def connect(self,host,port):
		if not self.socket:
			print ("connect")
			self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			try:
				self.socket.connect((str(host), int(port)))
			except Exception as e:
				print ("something's wrong with %s:%d. Exception is %s") % (str(host), int(port), e)
				self.socket.close()
				self.socket = None
				return

			self.host = host; self.socket.setblocking(0)

	def disconnect(self):
		if self.socket:
			print ("disconnect")
			self.socket.close()
			self.socket = None
			self.buffer = ''

	def poll(self):
		if self.socket:
			# print "poll"
			notready = False
			data = ''
			try:
				data = self.socket.recv(8192)
			except socket.error as e:
				err = e.args[0]
				if err == errno.EAGAIN or err == errno.EWOULDBLOCK:
					notready = True
				else:
					print ('Error'), err

			if len(data) == 0:
				if not notready:
					self.socket.close()
					self.socket = None
					print ("disconnected")
			else:
				self.gotdata(data)

		root.after(100,self.poll)

	def gotdata(self,data):
		self.buffer += data
		while True:
			if len(self.buffer) < 4:
				if len(self.buffer) != 0:
					print ("too short for length")
				break
			(length,) = struct.unpack(">i",self.buffer[0:4])
			if len(self.buffer) < length:
				print ("too short for data: buffer is"), len(self.buffer), "want", length
				break
			#print ("message length"),length,"buffer",len(self.buffer)
			m = self.buffer[4:length]
			self.gotmessage(m)
			# print "after got", len(self.buffer)
			self.buffer = self.buffer[length:]

	def gotmessage(self,message):
		#print ("Got Message:"),len(message)
		if len(message) < 4:
			print ("No Message header")
			return
		mtype = message[0]
		#print ("Got Message header: type"), mtype
		if mtype == 0:
			print ("JSON:")
			print (message[4:])
		elif mtype == 1:
			print ("Binary, more:"), ord(message[1])
			print (message[4:])
		else:
			#print ("Unhandled message type")
			print "Response from H1:" + message

	def send(self,message):
		#if self.socket:
		buffer = struct.pack(">i",len(message) + 4)
		#print ("Hexsending %s %s %s") % (binascii.hexlify(buffer), binascii.hexlify(message[0:4]), message[4:])
		#print ("Len %d" % (len(message)))
		#print ("Buf %d" % (len(buffer)))
		#print (message)
		self.socket.send(buffer + message)
			

	def sendcommand(self,message):
		h = struct.pack("BBBB",0,0,0,0)
		self.send(h + message)

	def sendjson(self,object):
		s = json.dumps(object)
		self.sendcommand(s)

h1 = H1()

class Connect(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.host = tk.StringVar(self,value='192.168.0.2')
		self.port = tk.StringVar(self,value='9999')

		self.hlabel = tk.Label(self, text="Host")
		self.hlabel.pack(side=tk.LEFT)
		self.hentry = tk.Entry(self,textvariable=self.host)
		self.hentry.pack(side=tk.LEFT)
		self.plabel = tk.Label(self, text="Port")
		self.plabel.pack(side=tk.LEFT)
		self.pentry = tk.Entry(self,textvariable=self.port)
		self.pentry.pack(side=tk.LEFT)
		self.connect_button = tk.Button(self, text="Connect", command=self.connect)
		self.connect_button.pack(side=tk.LEFT)
		self.disconnect_button = tk.Button(self, text="Disconnect", command=self.disconnect)
		self.disconnect_button.pack(side=tk.LEFT)

	def connect(self):
		h1.connect(self.host.get(),self.port.get())

	def disconnect(self):
		h1.disconnect()

class Send(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)
		self.message = tk.StringVar(self,value='{"command":"ping"}')
		self.mlabel = tk.Label(self,text="Message")
		self.mlabel.pack(side=tk.LEFT)
		self.mentry = tk.Entry(self,textvariable=self.message)
		self.mentry.pack(expand=True,fill='x',side=tk.LEFT)
		self.send_button = tk.Button(self, text="Send", command=self.send)
		self.send_button.pack(side=tk.LEFT)

	def send(self):
		h1.sendcommand(self.message.get().encode('utf-8'))

class Memory(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.size = tk.IntVar(self,value=512)

		self.mlabel = tk.Label(self,text="Memory Pool Size, MB")
		self.mlabel.pack(side=tk.LEFT)

		self.mentry = tk.Entry(self,textvariable=self.size)
		self.mentry.pack(side=tk.LEFT)

		self.init_button = tk.Button(self, text="Init", command=self.init)
		self.init_button.pack(side=tk.LEFT)

	def init(self):
		h1.sendjson({"command":"initpool","size":self.size.get()})

class InitCamera(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.camera = tk.IntVar(self,value=0)
		self.width = tk.IntVar(self,value=1920)
		self.height = tk.IntVar(self,value=1080)
		self.fps = tk.IntVar(self,value=30)
		self.gop = tk.IntVar(self,value=30)
		self.controlrate = tk.IntVar(self,value=2)
		self.bitrate = tk.IntVar(self,value=6000000)
		self.quality = tk.IntVar(self,value=1)
		self.buffersize = tk.IntVar(self,value=90)
		self.audioid = tk.IntVar(self,value=0)

		self.camera_label = tk.Label(self,text="camera")
		self.camera_label.pack(side=tk.LEFT)
		self.camera_entry = tk.Entry(self,textvariable=self.camera,width=2)
		self.camera_entry.pack(side=tk.LEFT)

		self.audioid_label = tk.Label(self,text="audioid")
		self.audioid_label.pack(side=tk.LEFT)
		self.audioid_entry = tk.Entry(self,textvariable=self.audioid,width=2)
		self.audioid_entry.pack(side=tk.LEFT)

		self.width_label = tk.Label(self,text="width")
		self.width_label.pack(side=tk.LEFT)
		self.width_entry = tk.Entry(self,textvariable=self.width,width=4)
		self.width_entry.pack(side=tk.LEFT)

		self.height_label = tk.Label(self,text="height")
		self.height_label.pack(side=tk.LEFT)
		self.height_entry = tk.Entry(self,textvariable=self.height,width=4)
		self.height_entry.pack(side=tk.LEFT)

		self.fps_label = tk.Label(self,text="fps")
		self.fps_label.pack(side=tk.LEFT)
		self.fps_entry = tk.Entry(self,textvariable=self.fps,width=2)
		self.fps_entry.pack(side=tk.LEFT)

		self.gop_label = tk.Label(self,text="gop")
		self.gop_label.pack(side=tk.LEFT)
		self.gop_entry = tk.Entry(self,textvariable=self.gop,width=2)
		self.gop_entry.pack(side=tk.LEFT)

		self.controlrate_label = tk.Label(self,text="controlrate")
		self.controlrate_label.pack(side=tk.LEFT)
		self.controlrate_entry = tk.Entry(self,textvariable=self.controlrate)
		self.controlrate_entry.pack(side=tk.LEFT)

		self.bitrate_label = tk.Label(self,text="bitrate")
		self.bitrate_label.pack(side=tk.LEFT)
		self.bitrate_entry = tk.Entry(self,textvariable=self.bitrate)
		self.bitrate_entry.pack(side=tk.LEFT)

		self.quality_label = tk.Label(self,text="quality")
		self.quality_label.pack(side=tk.LEFT)
		self.quality_entry = tk.Entry(self,textvariable=self.quality,width=2)
		self.quality_entry.pack(side=tk.LEFT)

		self.buffersize_label = tk.Label(self,text="buffersize")
		self.buffersize_label.pack(side=tk.LEFT)
		self.buffersize_entry = tk.Entry(self,textvariable=self.buffersize,width=4)
		self.buffersize_entry.pack(side=tk.LEFT)

		self.init_button = tk.Button(self,text="Init", command=self.init)
		self.init_button.pack(side=tk.LEFT)

	def init(self):
		h1.sendjson({
			"command":"recordinitcam",
			"camera":self.camera.get(),
			"width":self.width.get(),
			"height":self.height.get(),
			"fps":self.fps.get(),
			"gop":self.gop.get(),
			"controlrate":self.controlrate.get(),
			"bitrate":self.bitrate.get(),
			"quality":self.quality.get(),
			"buffersize":self.buffersize.get(),
			"audioid":self.audioid.get()
		})

class Login(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)
		self.officer = tk.StringVar(self,value='robertt')
		self.password = tk.StringVar(self,value='123')
		self.partner = tk.StringVar(self,value='')
		self.unit = tk.StringVar(self,value='BobsPrius')

		self.olabel = tk.Label(self,text="Officer")
		self.olabel.pack(side=tk.LEFT)
		self.oentry = tk.Entry(self,textvariable=self.officer)
		self.oentry.pack(side=tk.LEFT)
		self.passlabel = tk.Label(self,text="Password")
		self.passlabel.pack(side=tk.LEFT)
		self.passentry = tk.Entry(self,textvariable=self.password)
		self.passentry.pack(side=tk.LEFT)
		self.partnerlabel = tk.Label(self,text="Parter")
		self.partnerlabel.pack(side=tk.LEFT)
		self.partnerentry = tk.Entry(self,textvariable=self.partner)
		self.partnerentry.pack(side=tk.LEFT)
		self.unitlabel = tk.Label(self,text="Unit")
		self.unitlabel.pack(side=tk.LEFT)
		self.unitentry = tk.Entry(self,textvariable=self.unit)
		self.unitentry.pack(side=tk.LEFT)

		self.login_button = tk.Button(self, text="Login", command=self.login)
		self.login_button.pack(side=tk.LEFT)

	def login(self):
		h1.sendjson({"command":"login","officer":self.officer.get(),"password":self.password.get(),"partner":self.partner.get(),"unit":self.unit.get()})

class Status(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.label = tk.Label(self,text="Status")
		self.label.pack(side=tk.LEFT)

		self.get_button = tk.Button(self, text="Get", command=self.get)
		self.get_button.pack(side=tk.LEFT)

	def get(self):
		h1.sendjson({"command":"status"})

class Camera(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)
		self.camera = tk.IntVar(self,value=0)

		self.clabel = tk.Label(self,text="Camera")
		self.clabel.pack(side=tk.LEFT)

		self.centry = tk.Entry(self,textvariable=self.camera)
		self.centry.pack(side=tk.LEFT)

		self.rec_button = tk.Button(self, text="Record", command=self.record)
		self.rec_button.pack(side=tk.LEFT)

		self.stop_button = tk.Button(self, text="Stop", command=self.stop)
		self.stop_button.pack(side=tk.LEFT)

		self.preview_button = tk.Button(self, text="Preview", command=self.preview)
		self.preview_button.pack(side=tk.LEFT)

		self.stop_preview_button = tk.Button(self, text="Stop Preview", command=self.stop_preview)
		self.stop_preview_button.pack(side=tk.LEFT)

	def record(self):
		h1.sendjson({"command":"record","camera":self.camera.get()})

	def stop(self):
		h1.sendjson({"command":"stoprecord","camera":self.camera.get()})

	def preview(self):
		h1.sendjson({"command":"preview","camera":self.camera.get()})

	def stop_preview(self):
		h1.sendjson({"command":"stoppreview","camera":self.camera.get()})

class Server(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.name = tk.Label(self,text="Server")
		self.name.pack(side=tk.LEFT)

		self.start_button = tk.Button(self, text="Start", command=self.start)
		self.start_button.pack(side=tk.LEFT)

		self.stop_button = tk.Button(self, text="Stop", command=self.stop)
		self.stop_button.pack(side=tk.LEFT)

	def start(self):
		h1.sendjson({"command":"serverstart"})

	def stop(self):
		h1.sendjson({"command":"serverstop"})



class Live(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.camera = tk.IntVar(self,value=0)

		self.name = tk.Label(self,text="Live Stream")
		self.name.pack(side=tk.LEFT)

		self.clabel = tk.Label(self,text="Camera")
		self.clabel.pack(side=tk.LEFT)

		self.centry = tk.Entry(self,textvariable=self.camera)
		self.centry.pack(side=tk.LEFT)

		self.on_button = tk.Button(self, text="On", command=self.on)
		self.on_button.pack(side=tk.LEFT)

		self.off_button = tk.Button(self, text="Off", command=self.off)
		self.off_button.pack(side=tk.LEFT)

		self.view_button = tk.Button(self, text="View", command=self.view)
		self.view_button.pack(side=tk.LEFT)

	def on(self):
		h1.sendjson({"command":"livestream","camera":self.camera.get(),"on":True})

	def off(self):
		h1.sendjson({"command":"livestream","camera":self.camera.get(),"on":False})

	def view(self):
		spawnprocess(["vlc","http://" + h1.host + ":8080/live.ts"])

class FileStream(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.file = tk.StringVar('')

		self.name = tk.Label(self,text="File Stream")
		self.name.pack(side=tk.LEFT)

		self.fentry = tk.Entry(self,textvariable=self.file)
		self.fentry.pack(expand=True,fill='x',side=tk.LEFT)

		self.start_button = tk.Button(self, text="Start", command=self.start)
		self.start_button.pack(side=tk.LEFT)

		self.stop_button = tk.Button(self, text="Stop", command=self.stop)
		self.stop_button.pack(side=tk.LEFT)

		self.view_button = tk.Button(self, text="View", command=self.view)
		self.view_button.pack(side=tk.LEFT)

	def start(self):
		h1.sendjson({"command":"streamstartfile","filename":self.file.get()})

	def stop(self):
		h1.sendjson({"command":"streamstopfile"})

	def view(self):
		spawnprocess(["vlc","http://" + h1.host + ":8080/1.mp4"])


class Bookmark(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.camera = tk.IntVar(self,value=0)
		self.filename = tk.StringVar(self,value='foo.jpg')

		self.clabel = tk.Label(self,text="Camera")
		self.clabel.pack(side=tk.LEFT)

		self.centry = tk.Entry(self,textvariable=self.camera)
		self.centry.pack(side=tk.LEFT)

		self.book_button = tk.Button(self, text="Bookmark", command=self.bookmark)
		self.book_button.pack(side=tk.LEFT)

		self.flabel = tk.Label(self,text="File")
		self.flabel.pack(side=tk.LEFT)

		self.fentry = tk.Entry(self,textvariable=self.filename)
		self.fentry.pack(expand=True,fill='x',side=tk.LEFT)

		self.snap_button = tk.Button(self, text="Snapshot", command=self.snapshot)
		self.snap_button.pack(side=tk.LEFT)

	def bookmark(self):
		h1.sendjson({"command":"bookmark","camera":self.camera.get()})

	def snapshot(self):
		h1.sendjson({"command":"snapshot","camera":self.camera.get(),"filename":self.filename.get()})


class FileInfo(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.filename = tk.StringVar(self,value='xxx.mp4')

		self.flabel = tk.Label(self,text="File")
		self.flabel.pack(side=tk.LEFT)

		self.fentry = tk.Entry(self,textvariable=self.filename)
		self.fentry.pack(expand=True,fill='x',side=tk.LEFT)

		self.get_button = tk.Button(self, text="Get Info", command=self.getinfo)
		self.get_button.pack(side=tk.LEFT)

	def getinfo(self):
		h1.sendjson({"command":"getinfo","filename":self.filename.get()})

class GetFile(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.filename = tk.StringVar(self,value='units.config')

		self.flabel = tk.Label(self,text="File")
		self.flabel.pack(side=tk.LEFT)

		self.fentry = tk.Entry(self,textvariable=self.filename)
		self.fentry.pack(expand=True,fill='x',side=tk.LEFT)

		self.get_button = tk.Button(self, text="Get", command=self.get)
		self.get_button.pack(side=tk.LEFT)

	def get(self):
		h1.sendjson({"command":"readfile","filename":self.filename.get()})

class GetDir(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)

		self.dirname = tk.StringVar(self,value='/media/ubuntu/USB/cobanvideos')
		self.filter = tk.StringVar(self,value='*.mp4')

		self.dlabel = tk.Label(self,text="Directory")
		self.dlabel.pack(side=tk.LEFT)

		self.dentry = tk.Entry(self,textvariable=self.dirname)
		self.dentry.pack(expand=True,fill='x',side=tk.LEFT)

		self.flabel = tk.Label(self,text="Filter")
		self.flabel.pack(side=tk.LEFT)

		self.fentry = tk.Entry(self,textvariable=self.filter)
		self.fentry.pack(side=tk.LEFT)

		self.get_button = tk.Button(self, text="Get", command=self.get)
		self.get_button.pack(side=tk.LEFT)

	def get(self):
		h1.sendjson({"command":"ls","path":self.dirname.get(),"filters":[self.filter.get()]})

class Upload(tk.Frame):
	def __init__(self,parent):
		tk.Frame.__init__(self,parent)

		self.label = tk.Label(self,text="Upload")
		self.label.pack(side=tk.LEFT)

		self.get_button = tk.Button(self, text="Upload", command=self.upload)
		self.get_button.pack(side=tk.LEFT)

	def upload(self):
		h1.sendjson({"command":"upload", "icv":True})


class Main(tk.Frame):
	def __init__(self, parent):
		tk.Frame.__init__(self, parent)
		self.parent = parent
		self.parent.title("H1")
		self.connect = Connect(self)
		self.connect.pack()
		self.send = Send(self)
		self.send.pack(expand=True,fill='x')
		self.memory = Memory(self);
		self.memory.pack();
		self.initcamera = InitCamera(self)
		self.initcamera.pack()
		self.login = Login(self)
		self.login.pack()
		self.status = Status(self)
		self.status.pack()
		self.camera = Camera(self)
		self.camera.pack()
		self.upload = Upload(self)
		self.upload.pack()
		self.server = Server(self);
		self.server.pack()
		self.live = Live(self)
		self.live.pack()
		self.filestream = FileStream(self)
		self.filestream.pack(expand=True,fill='x')
		self.bookmark = Bookmark(self)
		self.bookmark.pack(expand=True,fill='x')
		self.fileinfo = FileInfo(self)
		self.fileinfo.pack(expand=True,fill='x')
		self.getfile = GetFile(self)
		self.getfile.pack(expand=True,fill='x')
		self.getdir = GetDir(self)
		self.getdir.pack(expand=True,fill='x')


def main():
	m = Main(root)
	m.pack(expand=True,fill='both')
	root.mainloop()

if __name__ == "__main__":
	main()



