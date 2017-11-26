#!/usr/bin/env python2.6
import getopt, sys, os, socket, signal, logging, bz2, struct, traceback, time, subprocess, shlex
# Written by MorgothV8: Lukasz Gryglicki, lukaszgryglicki@o2.pl, tel +48693582014
# globals
srv_sock=None
cli_sock=None
log_file='/tmp/PyVoice.log'
log_level=logging.INFO
glog=None
raw_audio_input='/dev/dsp'
raw_audio_output='/dev/dsp'
block_size=2048
use_oss_in=True
use_oss_out=True
n_read=0
n_sent=0
n_recieved=0
n_write=0
use_compress=True
dbg_mode=False
dbg_file_in='./recorded.raw'
dbg_file_out='./recieved.raw'
net_timeout=30
time_start=None
use_mixer=False
mixer_rec=90
mixer_mic=50
mixer_vol=40
mixer_pcm=40
#print('Audio format --afmt: 1=MULAW, 2=ALAW, 4=IMADPCM, 8=U8, 16=S16LE, 32=S16BE, 64=S8, 128=U16LE, 256=U16BE')
nbits=8
audio_fmt=1
#audio_fmt=ossaudiodev.AFMT_MU_LAW
#audio_fmt=ossaudiodev.AFMT_U8
#audio_fmt=ossaudiodev.AFMT_S16_LE
audio_chn=1
#audio_chn=2
#audio_spd=2000
#audio_spd=4000
#audio_spd=6000
audio_spd=8000
#audio_spd=11025
#audio_spd=22050
#audio_spd=44100
oggenc=False
ossaudiodev=None
sox=False
soxin_length=1.0
cygwin_mode=False
hdr_cut_from=0
hdr_cut_to=0
header_processed=False
cut_header=None
speexenc=False
speexq=0
sox_sfmt=None
speex_sfmtE=''
speex_sfmtD=''
ogg_sfmt=None
oggq=-1

# main code starts
def main():
	global log_file
	global log_level
	global raw_audio_input
	global raw_audio_output
	global block_size
	global use_oss_in
	global use_oss_out
	global use_compress
	global audio_fmt
	global audio_chn
	global audio_spd
	global mixer_rec
	global mixer_mic
	global mixer_vol
	global mixer_pcm
	global use_mixer
	global dbg_mode
	global oggenc
	global sox
	global ossaudiodev
	global nbits
	global soxin_length
	global cygwin_mode
	global hdr_cut_from
	global hdr_cut_to
	global speexenc
	global speexq
	global sox_sfmt
	global speex_sfmtE
	global speex_sfmtD
	global ogg_sfmt
	global oggq

	try:
	    optList, args = getopt.getopt(sys.argv[1:], 'QPZMLHOS12345gmfceioyhxas:p:l:u:d:b:X:', \
		    ['help', 'accept', 'server=', 'port=', 'logfile=', 'input', 'output', \
		    'audio_in=', 'audio_out=', 'blocksize', 'rawin', 'rawout', 'rawall', \
		    'full', 'afmt=', 'achn=', 'aspd=', 'mixrec=', 'mixmic=', 'mixvol=', \
		    'mixpcm=', 'mix', 'debug', 'low', 'high', 'ogg', 'sox', 'spm', 'soxin=', \
		    'zombie', 'chf=', 'cht=', 'defogg', 'speex', 'speexq=', 'oggq=', 'lowspeex', \
		    'zspx', 'stdspeex', 'stdogg', 'highspeex', 'cd'])
	except getopt.GetoptError as err:
		print 'Error: {0:s}'.format(err)
		print('Try --help or -h')
		sys.exit(1)
	port=9000
	server_name='localhost'
	listener=0
	Input=True
	DoubleMode=True
	for option, optVal in optList:
		if option == '--afmt':
			audio_fmt = int(optVal)
			print 'Audio format: {0:d}'.format(audio_fmt)
		elif option == '--achn':
			audio_chn = int(optVal)
			print 'Audio channels: {0:d}'.format(audio_chn)
		elif option == '--chf':
			hdr_cut_from = int(optVal)
			print 'Cut OGG header from byte {0:d}'.format(hdr_cut_from)
		elif option == '--cht':
			hdr_cut_to = int(optVal)
			print 'Cut OGG header to byte {0:d}'.format(hdr_cut_to)
		elif option == '--defogg':
			hdr_cut_from = 96
			hdr_cut_to = 2048
			oggenc=True
			oggq=-1
			block_size=8192
			print 'Cut OGG header is enabled in range {0:d} - {1:d}, quality -1, buffer 8K'.format(hdr_cut_from, hdr_cut_to)
		elif option == '--stdogg':
			hdr_cut_from = 96
			hdr_cut_to = 2048
			oggenc=True
			oggq=3
			block_size=8192
			audio_fmt=16
			audio_spd=16384
			print 'Standard Ogg header cut {0:d} - {1:d}, quality 3, buffer 8K, fmt s16le, 16.4 kHz'.format(hdr_cut_from, hdr_cut_to)
		elif option == '--lowspeex':
			hdr_cut_from = 134
			hdr_cut_to = 182
			speexenc=True
			use_compress=False
			speexq=0
			block_size=4096
			audio_spd=7000
			audio_fmt=8
			print 'Speex low mode, quality 0, buffer 4K, fmt u8, 7 kHz'
		elif option == '--stdspeex':
			hdr_cut_from = 134
			hdr_cut_to = 182
			speexenc=True
			use_compress=False
			speexq=1
			block_size=4096
			audio_spd=12288
			audio_fmt=8
			print 'Speex standard mode, quality 1, buffer 4K, fmt u8, 12.3 kHz kHz'
		elif option == '--highspeex':
			hdr_cut_from = 134
			hdr_cut_to = 182
			speexenc=True
			use_compress=False
			speexq=4
			block_size=2048
			audio_spd=16384
			audio_fmt=8
			print 'Speex high mode, quality 4, buffer 2K, fmt U8, 16.4 kHz kHz'
		elif option in ('--ogg', '-O'):
			oggenc=True
			print'Ogg Encoding: on, suggested: block>=8K, afmt=U8, achn=1, aspd=8K, soxinlen=1.0s expect 1s LAG'
		elif option == '--oggq':
			oggq = int(optVal)
			print 'OGG quality: {0:d}'.format(oggq)
		elif option in ('--speex', '-P'):
			speexenc=True
			print'Speex Encoding: on, suggested: block>=4K, afmt=U8|S16LE, achn=1, aspd=6-32K'
		elif option in ('--speexq', '-Q'):
			speexq = int(optVal)
			print 'Speex quality: {0:d}'.format(speexq)
		elif option in ('--zombie', '-Z'):
			oggenc=True
			use_compress=True
			block_size=16384
			audio_fmt=8
			audio_chn=1
			audio_spd=4096
			soxin_length=4.0
			hdr_cut_from=96
			hdr_cut_to=2048
			print'*ZOMBIE* method is only for emergency, quality and lags would be *TERRIBLE*'
			print'You may also try it in SoX mode, in all modes lag would be about 8s, so suggest one way patient communication'
			print'It records in 4096Hz, U8, mono, waits until 16K buffer fill, encodes it with Ogg at -1 quality'
			print'Then compresses with BZip2 at level 9, and finally sent through network'
			print'Audio compression + bzip compression can get some time, so audio buffer may not recieve data in time'
			print'So every noises etc also possible, filling 16K buffer with 4K sample takes at least 4s'
			print'Buffer must be 16K becouse of better Ogg compression - there is constant size Ogg header'
		elif option == '--zspx':
			speexenc=True
			use_compress=False
			block_size=16384
			audio_fmt=8
			audio_chn=1
			audio_spd=6000
			soxin_length=4.0
			hdr_cut_from = 134
			hdr_cut_to = 182
			print'Speex, BZ2, buffer 16K, U8 6 kHz, mono: minimum BPS possible at all - 3s LAG'
		elif option in ('--sox', '-S'):
			sox=True
			use_oss_in=False
			use_oss_out=False
			print'Using SoX for sound I/O: by default it uses 1s record buffer, expect 1s LAG'
			print'You may manipulate record buffer, but values < 1 seems to be incorrect'
		elif option in ('--soxin', '-X'):
			soxin_length = float(optVal)
			print'SoX Rec buffer : {0:3.2f}s'.format(soxin_length)
		elif option in ('--spm', '-M'):
			server_name='88.199.169.19'
			port=9000
			print'Using SPM -Mechanik- server'
		elif option == '--aspd':
			audio_spd = int(optVal)
			print'Audio speed: {0:d}'.format(audio_spd)
		elif option == '--mixrec':
			mixer_rec = int(optVal)
			print'Mixer REC: {0:d}'.format(mixer_rec)
		elif option == '--mixmic':
			mixer_mic = int(optVal)
			print'Mixer MIC: {0:d}'.format(mixer_mic)
		elif option == '--mixvol':
			mixer_vol = int(optVal)
			print'Mixer VOL: {0:d}'.format(mixer_vol)
		elif option == '--mixpcm':
			mixer_pcm = int(optVal)
			print'Mixer PCM:{0:d}'.format(mixer_pcm)
		elif option in ('-h', '--help'):
			help_usage()
			sys.exit(0)
		elif option in ('-L', '--low'):
			print'Low quality mode'
			audio_fmt=8
			audio_chn=1
			audio_spd=4096
			block_size=4096
		elif option in ('-H', '--high'):
			print'High quality mode'
			audio_fmt=16
			audio_chn=1
			audio_spd=22050
			block_size=1024
		elif option == '--cd':
			print'CD stereo max possible quality mode'
			use_compress=False
			audio_fmt=16
			audio_chn=2
			audio_spd=44100
			block_size=512
		elif option in ('-m', '--mix'):
			use_mixer=True
			print'Using mixer settings'
		elif option in ('-g', '--debug'):
			dbg_mode=True
			print'Using debug file(s)'
		elif option in ('-p', '--port'):
			port=int(optVal)
			print'Port: {0:d}'.format(port)
		elif option in ('-s', '--server'):
			server_name=optVal
			print'Server: {0:s}'.format(server_name)
		elif option in ('-u', '--audio_in'):
			raw_audio_input=optVal
			print'Audio: {0:s}'.format(raw_audio_input)
		elif option in ('-d', '--audio_out'):
			raw_audio_output=optVal
			print'DSP: {0:s}'.format(raw_audio_output)
		elif option in ('-a', '--accept'):
			print'Acceptor mode on'
			listener=1
		elif option in ('-l', '--logfile'):
			log_file=optVal
			print'Log: {0:s}'.format(log_file)
		elif option in ('-y', '--full'):
			use_compress=False
			print'Uncompressed mode on'
		elif option in ('-c', '--rawin'):
			use_oss_in=False
			audio_fmt=8
			audio_chn=1
			audio_spd=8000
			print'RAW input mode on'
		elif option in ('-e', '--rawout'):
			use_oss_out=False
			audio_fmt=8
			audio_chn=1
			audio_spd=8000
			print'RAW output mode on'
		elif option in ('-f', '--rawall'):
			use_oss_in=False
			use_oss_out=False
			audio_fmt=8
			audio_chn=1
			audio_spd=8000
			print'RAW all mode on'
		elif option in ('-b', '--blocksize'):
			block_size=int(optVal)
			print'BlockSize: {0:d}'.format(block_size)
		elif option in ('-i', '--input'):
			print'Input mode only'
			Input=True
		elif option in ('-o', '--output'):
			print'Output mode only'
			Input=False
		elif option == '-x':
			print'Bidirection mode off'
			DoubleMode=False
		elif option == '-1':
			print'Loging at DEBUG level'
			log_level=logging.DEBUG
		elif option == '-2':
			print'Loging at INFO level'
			log_level=logging.INFO
		elif option == '-3':
			print'Loging at WARNING level'
			log_level=logging.WARNING
		elif option == '-4':
			print'Loging at ERROR level'
			log_level=logging.ERROR
		elif option == '-5':
			print'Loging at CRITICAL ERROR level'
			log_level=logging.CRITICAL
		else:
			print'Unknown option'
			help_usage()
			sys.exit(1)

	print'System: {0:s}-{1:s}'.format(os.name, sys.platform)

	if sys.platform == 'win32':
		print'Windows platform supported only in Cygwin mode, win32 doesnt have some signals and fork'
		print'Cannot operate in such invalid environment, aborting...'
		cygwin_mode=True
		exit(1)

	if sys.platform == 'cygwin':
		print'Cygwin OS detected, disabling raw input and OSS input'
		print'Only SoX possible by now in Cygwin'
		print'Cygwin mode on, SoX mode on, RAW mode off, OSS mode off'
		sox=True
		use_oss_in=False
		use_oss_out=False
		cygwin_mode=True

	else:
		if use_oss_in or use_oss_out:
			print'Importing UNIX OSS module'
			import ossaudiodev

	signal.signal(signal.SIGINT, sighandler)
	signal.signal(signal.SIGTERM, sighandler)
	#if not cygwin_mode:
	if True:
		signal.signal(signal.SIGHUP, sighandler)
		signal.siginterrupt(signal.SIGHUP, False)
		signal.signal(signal.SIGALRM, sighandler)

	if audio_fmt == 16:
		nbits = 16
		sox_sfmt = 'signed-integer'
		#speex_sfmtE = ' -w'
		#speex_sfmtD = ' --force-wb'
		#speex_sfmtD = ' '
		ogg_sfmt = ' -e 0 -s 1'
	else:
		nbits = 8
		sox_sfmt = 'unsigned-integer'
		#speex_sfmtE = ' -n'
		#speex_sfmtD = ' --force-nb'
		ogg_sfmt = ' -e 0 -s 0'
		if audio_fmt == 1:
			if oggenc or speexenc:
				print'Cannot encode Ogg or Speex as MU-LAW, changinbg to U8'
				audio_fmt=8
			else:
				sox_sfmt = "mu-law"

	if speexenc:
		if audio_spd < 6000:
			print'Audio speed {0:d} too low for Speex, adjusting to 6K'.format(audio_spd)
			audio_spd = 6000

		if audio_spd > 32000:
			print'Audio speed {0:d} too high for Speex, adjusting to 32K'.format(audio_spd)
			audio_spd = 32000
			

	if listener == 1:
		pid1 = server(port)
		print'Server PID: {0:d}'.format(pid1)
	else:
		if DoubleMode:
			pid1 = client(server_name, port, True)
			pid2 = client(server_name, port, False)
			print'In/Out clients PIDS: {0:d}, {1:d}'.format(pid1, pid2)
		else:
			pid1 = client(server_name, port, Input)
			print'Client PID: {0:d}'.format(pid1)
	
	sys.exit(0)

def my_waitpid(pid, opt):

	glog.debug('Wait for pid %d, option %d', pid, opt)
	if opt == os.WNOHANG:
		glog.debug('Fast wait - no hang')
		os.waitpid( pid, opt )
		return

	if pid <= 0:
		glog.debug('No waiting for 0 PID')
		return

	signal.alarm(net_timeout)
	os.waitpid( pid, opt )
	signal.alarm(0)
	glog.debug('Wait finished')

def my_send(sock, bstr, n):
	nsend = 0
	signal.alarm(net_timeout)
	while nsend < n:
		nsend += sock.send(bstr[nsend:])
	
	signal.alarm(0)

def my_recv(sock, n):
	nrecv = 0
	bstr = b''
	signal.alarm(net_timeout)
	while nrecv < n:
		tmpstr = sock.recv(n-nrecv)
		nrecv += len(tmpstr)
		bstr += tmpstr
	signal.alarm(0)
	return bstr

def client(server, port, Input):
	global log_file
	global cli_sock

	try:
		pid = os.fork()
		if pid > 0:
			return pid
	except OSError as ex:
		print'fork() error: errno: {0:d}: {1:s}'.format(ex.errno, ex.strerror)
		glog.critical('fork() error: %d %s', ex.errno, ex.strerror)
		sys.exit(1)

	#print('Client connect to: ', server, port, ', input mode: ', Input)

	log_file+='.client'
	setup_logs()

	try:
		client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		client_socket.connect((server, port))
	except socket.error as ex:
		print'socket() error: {0:s}'.format(ex)
		glog.critical('socket() error: %s', str(ex))
		sys.exit(1)
	#print('Connected to server')
	glog.info('Connected to %s:%d', server, port)
	cli_sock=client_socket
	if Input:
		glog.debug('Input mode client')
		my_send(client_socket, b'I', 1)
	else:
		glog.debug('Output mode client')
		my_send(client_socket, b'O', 1)
	my_recv(client_socket, 1)
	#print('Server says: ', str(data))

	if Input:
		input_sender(client_socket)
	else:
		my_send(client_socket, b'K', 1)
		input_reciever(client_socket)

def setup_mixer():

	if not use_mixer:
		glog.info('Skipping mixer settings')
		return

	scmd = 'mixer rec ' + str(mixer_rec) + ' > /dev/null'
	os.system( scmd )

	scmd = 'mixer mic ' + str(mixer_mic) + ' > /dev/null'
	os.system( scmd )

	scmd = 'mixer vol ' + str(mixer_vol) + ' > /dev/null'
	os.system( scmd )

	scmd = 'mixer pcm ' + str(mixer_pcm) + ' > /dev/null'
	os.system( scmd )


	glog.info('Mixer input set: rec=%d, mic=%d', mixer_rec, mixer_mic)
	glog.info('Mixer output set: vol=%d, pcm=%d', mixer_vol, mixer_pcm)


def soxin():

	scmd = 'sox -d -q -b ' + str(nbits) + ' -r ' + str(audio_spd) + ' -c ' + str(audio_chn) + ' -e ' + sox_sfmt + ' -t raw - trim 0 ' + str(soxin_length)
	glog.debug( scmd )
	sargs = shlex.split( scmd )
	datalen = (nbits * audio_chn * audio_spd) / 8
	aerr = ''
	try:
		p = subprocess.Popen( args=sargs, bufsize=datalen, stdout=subprocess.PIPE, stderr=subprocess.PIPE )
		(astr, aerr) = p.communicate()
	except OSError as ex:
		print'subprocess.Popen() error: Errno: {0:d}: {1:s}'.format(ex.errno, ex.strerror)
		print'subprocess.Popen() error: Errno: {0:s}'.format(str(sargs))
		glog.critical('subprocess.Popen() error: %d %s %s', ex.errno, ex.strerror, sargs)
		if aerr <> '':
			glog.error( aerr )
		return ''

	if aerr <> '':
		glog.warning( aerr )

	glog.debug('SoX recorded %d bytes', len(astr))
	return astr

def soxout( adata ):

	try:
		pid = os.fork()
		if pid > 0:
			return pid
	except OSError as ex:
		print'fork() error: errno: {0:d}: {1:s}'.format(ex.errno, ex.strerror)
		glog.critical('fork() error: %d %s', ex.errno, ex.strerror)
		sys.exit(1)

	scmd = 'sox -q -b ' + str(nbits) + ' -r ' + str(audio_spd) + ' -c ' + str(audio_chn) + ' -e ' + sox_sfmt + ' -t raw - -d'
	glog.debug( scmd )
	sargs = shlex.split( scmd )
	datalen = (nbits * audio_chn * audio_spd) / 8
	aerr = ''
	try:
		p = subprocess.Popen( args=sargs, bufsize=datalen, stdin=subprocess.PIPE, stderr=subprocess.PIPE )
		(astr, aerr) = p.communicate( adata )
	except OSError as ex:
		glog.critical('subprocess.Popen() error: %d %s %s', ex.errno, ex.strerror, sargs)
		glog.critical('Audio was not played: process should be killed')
		if aerr <> '':
			glog.error( aerr )
		sys.exit(1)

	if aerr <> '':
		glog.warning( aerr )

	glog.debug('SoX played %d bytes', len(adata))
	sys.exit(0)

def cut_in_header( adata ):

	global header_processed
	global cut_header

	if hdr_cut_from <= 0:
		return adata

	if not header_processed:
		if len(adata) < hdr_cut_to:
			return adata
		cut_header = adata[hdr_cut_from:hdr_cut_to]
		header_processed = True
		glog.debug('OggIn: Remembered Ogg header from %d to %d, len %d, all %d', hdr_cut_from, hdr_cut_to, len(cut_header), len(adata))
		#print'Input1 parts before: {0:s} ... {1:s}'.format(adata[:8], adata[hdr_cut_from-4:hdr_cut_from+4])
		#print'Input1 parts cut   : {0:s} ... {1:s}'.format(adata[hdr_cut_from-4:hdr_cut_from+4], adata[hdr_cut_to-4:hdr_cut_to+4])
		#print'Input1 parts after : {0:s} ... {1:s}'.format(adata[hdr_cut_to-4:hdr_cut_to+4], adata[-8:])
		#print'Input1 header      : {0:s} ... {1:s}'.format(cut_header[:8], cut_header[-8:])
		return adata

	ogg_data = adata[:hdr_cut_from] + adata[hdr_cut_to:]
	glog.debug('OggIn: Sending cut data, len %d, all %d', len(ogg_data), len(adata))
	#print'Input2 parts before: {0:s} ... {1:s}'.format(ogg_data[:8], ogg_data[hdr_cut_from-4:hdr_cut_from+4])
	#print'Input2 parts cut   : {0:s} ... {1:s}'.format(cut_header[:8], cut_header[-8:])
	#print'Input2 parts after : {0:s} ... {1:s}'.format(ogg_data[hdr_cut_from-4:hdr_cut_from+4], ogg_data[-8:])
	#print'Input2 header      : {0:s} ... {1:s}'.format(cut_header[:8], cut_header[-8:])
	return ogg_data

def cut_out_header( adata ):

	global header_processed
	global cut_header

	if hdr_cut_from <= 0:
		return adata

	if not header_processed:
		if len(adata) < hdr_cut_to:
			return adata
		cut_header = adata[hdr_cut_from:hdr_cut_to]
		header_processed = True
		glog.debug('OggOut: Remembered Ogg header from %d to %d, len %d, all %d', hdr_cut_from, hdr_cut_to, len(cut_header), len(adata))
		return adata

	ogg_data = adata[:hdr_cut_from] + cut_header + adata[hdr_cut_from:]
	glog.debug('OggOut: Using uncut data, len %d, cut %d', len(ogg_data), len(adata))
	return ogg_data

def ogg_encode( adata ):

	scmd = 'oggenc -r -q ' + str(oggq) + ' -Q -B ' + str(nbits) + ' -R ' + str(audio_spd) + ' -C ' + str(audio_chn) + ' - -o -'
	glog.debug( scmd )
	sargs = shlex.split( scmd )
	datalen = (nbits * audio_chn * audio_spd) / 8
	try:
		p = subprocess.Popen( args=sargs, bufsize=datalen, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE )
		(astr, aerr) = p.communicate( adata )
	except OSError as ex:
		glog.error('subprocess.Popen() error: %d %s %s', ex.errno, ex.strerror, sargs)
		glog.error('Continuying without OGG compressor/decompressor - if it failed both sides, it will work without ogg compression')
		if aerr <> '':
			glog.critical( aerr )
		return adata
	
	if aerr <> '':
		glog.error( aerr )

	astr = cut_in_header( astr )
	glog.debug('OggEnc %d --> %d bytes', len( adata ), len( astr ))
	return astr

def ogg_decode( adata ):

	adata = cut_out_header( adata )

	scmd = 'oggdec -R -Q -b ' + str(nbits) + ogg_sfmt + ' - -o -'
	glog.debug( scmd )
	sargs = shlex.split( scmd )
	datalen = (nbits * audio_chn * audio_spd) / 8
	aerr = ''
	try:
		p = subprocess.Popen( args=sargs, bufsize=datalen, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE )
		(astr, aerr) = p.communicate( adata )
	except OSError as ex:
		glog.error('subprocess.Popen() error: %d %s %s', ex.errno, ex.strerror, sargs)
		glog.error('Continuying without OGG compressor/decompressor - if it failed both sides, it will work without ogg compression')
		if aerr <> '':
			glog.critical( aerr )
		return adata

	if aerr <> '':
		glog.error( aerr )

	glog.debug('OggDec %d --> %d bytes', len( adata ), len( astr ))
	return astr

def speex_encode( adata ):

	scmd = 'speexenc --quiet --rate ' + str(audio_spd) + ' --le --' + str(nbits) + 'bit --quality ' +str(speexq) + ' --vbr --dtx ' + speex_sfmtE

	if audio_chn == 2:
		scmd += ' --stereo'
	
	scmd += ' - -'
	glog.debug( scmd )
	sargs = shlex.split( scmd )
	datalen = (nbits * audio_chn * audio_spd) / 8
	aerr = ''
	try:
		p = subprocess.Popen( args=sargs, bufsize=datalen, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE )
		(astr, aerr) = p.communicate( adata )
	except OSError as ex:
		glog.error('subprocess.Popen() error: %d %s %s', ex.errno, ex.strerror, sargs)
		glog.error('Continuying without OGG compressor/decompressor - if it failed both sides, it will work without ogg compression')
		if aerr <> '':
			glog.critical( aerr )
		return adata
	
	if aerr <> '':
		glog.error( aerr )

	astr = cut_in_header( astr )
	glog.debug('SpeexEnc %d --> %d bytes', len( adata ), len( astr ))
	return astr

def speex_decode( adata ):

	adata = cut_out_header( adata )

	scmd = 'speexdec --quiet --enh --rate ' + str(audio_spd) + ' ' + speex_sfmtD

	if audio_chn == 2:
		scmd += ' --stereo'
	else:
		scmd += ' --mono'

	scmd += ' - -'
	glog.debug( scmd )
	sargs = shlex.split( scmd )
	datalen = (nbits * audio_chn * audio_spd) / 8
	aerr = ''
	try:
		p = subprocess.Popen( args=sargs, bufsize=datalen, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE )
		(astr, aerr) = p.communicate( adata )
	except OSError as ex:
		glog.error('subprocess.Popen() error: %d %s %s', ex.errno, ex.strerror, sargs)
		glog.error('Continuying without OGG compressor/decompressor - if it failed both sides, it will work without ogg compression')
		if aerr <> '':
			glog.critical( aerr )
		return adata

	if aerr <> '':
		glog.error( aerr )

	glog.debug('SpeexDec %d --> %d bytes', len( adata ), len( astr ))
	return astr
def input_sender(sock):
	global n_read
	global n_sent
	global use_compress
	global time_start

	try:
		if use_oss_in:
			glog.info('Using OSS driver for reading')
			audio = ossaudiodev.open('r')

			fmt = audio.setfmt( audio_fmt )
			chn = audio.channels( audio_chn )
			spd = audio.speed( audio_spd )

			glog.info('Opened OSS driver for: %s %s', audio.name, audio.mode)
			glog.info('Params %s %s %s, requested %s %s %s', fmt, chn, spd, audio_fmt, audio_chn, audio_spd)

			if fmt != audio_fmt or chn != audio_chn or spd != audio_spd:
				print('Failed to set audio parameters')
				glog.critical('Failed to set audio parameters')
				sock.close()
				sys.exit(1)

			#not used
			#mixer=ossaudiodev.openmixer()
			#mixer.set(ossaudiodev.SOUND_MIXER_REC, (100, 100))
			#mixer.set(ossaudiodev.SOUND_MIXER_MIC, (100, 100))
			#mixer.set_recsrc( 1 << ossaudiodev.SOUND_MIXER_MIC)
			#glog.info('Mixer configured')
		elif not sox:
			glog.info('Opening audio input: %s', raw_audio_input)
			audio = open(raw_audio_input, 'rb')
		#debug
		if dbg_mode:
			glog.info('Opening recording debug file: %s', dbg_file_in)
			outf = open(dbg_file_in, 'wb')

	except IOError as estr:

		print('Cannot open audio device: ', raw_audio_input, estr)
		glog.critical('Cannot open audio file: %s, %s', raw_audio_input, estr)
		glog.critical('Possible bad parameters requested: %s %s %s', audio_fmt, audio_chn, audio_spd)
		sock.close()
		sys.exit(1)

	#glog.info('Setup mixer mic rec=%d mic=%d, out vol=%d pcm=%d', mixer_rec, mixer_mic, mixer_vol, mixer_pcm)
	setup_mixer()
	time_start = time.time()
	glog.info('Begining audio transfer')
	nsteps = 0

	while True:
		try:
			if sox:
				adata = soxin()
			else:
				adata = audio.read(block_size)

		except IOError as ex:
			glog.critical('IOError audio.read: %s', str(ex))
			sock.close()
			sys.exit(1)

		if not adata:
			glog.warning('EOF reached on input')
			blen = struct.pack('=L', 0)
			my_send(sock, blen, 4)
			my_recv(sock, 1)
			sock.close()
			sys.exit(1)

		glog.debug('Read block %d bytes', len(adata))

		if speexenc:
			origdata = adata
			adata = speex_encode( adata )

		elif oggenc:
			origdata = adata
			adata = ogg_encode( adata )
		else:
			origdata = adata

		if use_compress:
			cmpr = bz2.BZ2Compressor()
			cadata = cmpr.compress(adata)
			cadata += cmpr.flush()
			del cmpr
		else:
			cadata = adata

		blen = struct.pack('=L', len(cadata))

		try:
			my_send(sock, blen, 4)
			my_recv(sock, 1)
			my_send(sock, cadata, len(cadata))
			my_recv(sock, 1)
			n_read += len(adata)
			n_sent += len(cadata)
			nsteps += 1
			if (nsteps % 1000) == 0:
				traffic_stats()

			if dbg_mode:
				outf.write(origdata)
			#print(len(cadata))

			glog.debug('Send %d bytes', len(cadata))

		except socket.error as ex:
			glog.critical('socket() error: %s', str(ex))
			sock.close()
			sys.exit(1)

def handle_client(s):
	try:
		pid = os.fork()
		if pid > 0:
			return pid
	except OSError as ex:
		print'fork() error: errno: {0:d}: {1:s}'.format(ex.errno, ex.strerror)
		glog.critical('fork() error: %d %s', ex.errno, ex.strerror)
		sys.exit(1)

	cli_type = str(my_recv(s, 1))
	#print('cli_type', cli_type)
	glog.info('Client type is %s', cli_type)

	if cli_type == str(b'I'):
		glog.debug('Input client')
		Input=True
	elif cli_type == str(b'O'):
		glog.debug('Output client')
		Input=False
	else:
		glog.error('Unknown client type: %s', cli_type)
		#my_send(s, b'!', 1)
		return False

	glog.debug('Input: %s', Input)
	my_send(s, b'K', 1)

	if Input:
		glog.info('Server in recieve mode')
		input_reciever(s)
	else:
		glog.info('Server in send mode')
		my_recv(s, 1)
		input_sender(s)

	glog.info('Client finished')
	sys.exit(0)

def input_reciever(sock):
	global n_write
	global n_recieved
	global use_compress
	global time_start
	global audio_fmt
	global nbits
	global sox_sfmt
	global speex_sfmtE
	global speex_sfmtD
	global ogg_sfmt

	try:
		if speexenc and audio_fmt <> 16:
			#print'SpeexDecoder can use only 16bit sound, adjusting output driver'
			glog.info('SpeexDecoder can use only 16bit sound, adjusting output driver')
			audio_fmt = 16
			nbits = 16
			sox_sfmt = 'signed-integer'
			#speex_sfmtE = ' -w'
			#speex_sfmtD = ' '
			ogg_sfmt = ' -e 0 -s 1'

		if use_oss_out:
			glog.info('Using OSS driver for writing')
			dsp = ossaudiodev.open('w')

			fmt = dsp.setfmt( audio_fmt )
			chn = dsp.channels( audio_chn )
			spd = dsp.speed( audio_spd )

			glog.info('Opened OSS driver for: %s %s', dsp.name, dsp.mode)
			glog.info('Params %s %s %s, requested %s %s %s', fmt, chn, spd, audio_fmt, audio_chn, audio_spd)

			if fmt != audio_fmt or chn != audio_chn or spd != audio_spd:
				print'Failed to set audio parameters'
				glog.critical('Failed to set audio parameters')
				sock.close()
				sys.exit(1)

		elif not sox:
			glog.info('Opening DSP file: %s', raw_audio_output)
			dsp = open(raw_audio_output, 'wb')

		if dbg_mode:
			glog.info('Opening play debug file: %s', dbg_file_out)
			outf = open(dbg_file_out, 'wb')

	except IOError as estr:
		glog.critical('Cannot open dsp file: %s, %s', raw_audio_output, estr)
		glog.critical('Possible bad parameters requested: %s %s %s', audio_fmt, audio_chn, audio_spd)
		sock.close()
		sys.exit(1)

	#glog.info('Setup mixer mic rec=%d mic=%d, out vol=%d pcm=%d', mixer_rec, mixer_mic, mixer_vol, mixer_pcm)
	setup_mixer()
	time_start = time.time()
	glog.info('Start recieving of audio')
	nsteps = 0
	pid = 0

	while True:
		try:
			blen = my_recv(sock, 4)
			if not blen:
				glog.warning('Client finished')
				sock.close()
				sys.exit(0)
			l = struct.unpack('=L', blen)
			#print(l[0])
			my_send(sock, b'>', 1)
			bufsize = l[0]
			bbytes = my_recv(sock, bufsize)
			glog.debug('Length %d, Read %d', l[0], len(bbytes))

			if len(bbytes) < 4:
				glog.warning('Client finished')
				sock.close()
				sys.exit(0)

			#if use_compress and bbytes[0] != 66:
			#	bbytes = b'B' + bbytes
			#	glog.warning('Changed bytes: %s', str(bbytes[:10]))

			#print(len(bbytes))
			my_send(sock, b'>', 1)

		except socket.error as ex:
			glog.critical('socket() error: %s', str(ex))
			sock.close()
			sys.exit(1)

		except struct.error as ex:
			glog.critical('unpack() error: %s', str(ex))
			sock.close()
			sys.exit(1)

		if len(bbytes) > 0:
			if use_compress:
				dbytes=''
				cmpr = bz2.BZ2Decompressor()
				try:
					dbytes = cmpr.decompress(bbytes)
					del cmpr
				except IOError as estr:
					#print('Decompression error:', estr)
					glog.warning('Decompression error: %s', estr)
				glog.debug('Decompressed %d bytes', len(dbytes))
			else:
				dbytes = bbytes
				glog.debug('Recieved %d bytes', len(dbytes))

			if speexenc:
				dbytes = speex_decode( dbytes )

			elif oggenc:
				dbytes = ogg_decode( dbytes )

			try:
				if sox:
					my_waitpid(pid, 0)
					pid = soxout( dbytes)
					glog.debug('New SoX Pid %d', pid)
				else:
					dsp.write(dbytes)

			except IOError as ex:
				glog.critical('IOError: dsp.write: %s', str(ex))
				sock.close()
				sys.exit(1)

			n_recieved += len(bbytes)
			n_write += len(dbytes)
			nsteps += 1
			if (nsteps % 1000) == 0:
				traffic_stats()

			if dbg_mode:
				outf.write(dbytes)

def server(port):
	global srv_sock
	global log_file

	log_file+='.server'
	setup_logs()

	try:
		pid = os.fork()
		if pid > 0:
			return pid
	except OSError as ex:
		print'fork() error: errno: {0:d}: {1:s}'.format(ex.errno, ex.strerror)
		glog.critical('fork() error: %d %s', ex.errno, ex.strerror)
		sys.exit(1)
	try:
		server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		server_socket.bind(('',port))
		server_socket.listen(5)
	except socket.error as ex:
		print'socket() error: {0:s}'.format(ex)
		glog.critical('socket() error: %s', str(ex))
		sys.exit(1)
	running = True
	srv_sock = server_socket
	while running:
		try:
			glog.debug('Wait for connection...')
			s, address = server_socket.accept()
			glog.info('Got client %s', str(address))
			pid = handle_client(s)
			glog.info('Client PID: %d', pid)
			my_waitpid(0, os.WNOHANG)
		except socket.error as ex:
			#print('socket() error: ', ex)
			glog.error('socket() error: %s', str(ex))

def traffic_stats():

	if not time_start:
		return
	
	time_end = time.time()
	tdiff = time_end - time_start
	glog.info('Elapsed time %f', tdiff)

	if n_read > 0 and n_sent > 0:
		glog.info('Bytes read %d', n_read)
		bps = n_read / tdiff
		glog.info('BPS uncompressed %f', bps)

		glog.info('Bytes sent %d', n_sent)
		bps = n_sent / tdiff
		glog.info('BPS %f', bps)

		bps = (n_sent * 100.0) / n_read
		glog.info('Compression %f', bps)

	if n_write > 0 and n_recieved > 0:
		glog.info('Bytes recieved %d', n_recieved)
		bps = n_recieved / tdiff
		glog.info('BPS %f', bps)

		glog.info('Bytes written %d', n_write)
		bps = n_write / tdiff
		glog.info('BPS uncompressed %f', bps)

		bps = (n_recieved * 100.0) / n_write
		glog.info('Compression %f', bps)

	if n_sent > 0 and n_recieved > 0:
		bps = (n_sent + n_recieved) / tdiff
		glog.info('Traffic BPS: %f', bps)

def sighandler(signum, frame):

	if signum == signal.SIGHUP:
		glog.warning('Got signal %d', signum)
		traffic_stats()
		return

	glog.warning('Got signal %d quiting', signum)
	traffic_stats()

	if signum == signal.SIGALRM:
		glog.critical('Current operation timeout, exiting')
		#glog.critical('Frame: %s(): %s', frame.f_code.co_name, frame.f_lineno)
		for sfile, sline, sfunc, sdummy in traceback.extract_stack(frame):
		    glog.critical( 'Traceback: File: Line: %s, %s Function: %s', sfile, sline, sfunc)

	if srv_sock:
		glog.debug('Closing server socket')
		srv_sock.close()

	if cli_sock:
		glog.debug('Closing client socket')
		cli_sock.close()

	sys.exit(0)

def setup_logs():
	global log_file
	global log_level
	global glog

	logger = logging.getLogger('PyVoice')
	logger.setLevel(log_level)
	ch = logging.FileHandler(log_file, mode='a')
	fmt = logging.Formatter('%(asctime)s: %(process)d: %(levelname)s: %(message)s')
	ch.setFormatter(fmt)
	logger.addHandler(ch)

	#logging.basicConfig(filename=log_file, level=log_level)
	logger.info('Otwarto log: %s', log_file)
	glog = logger
	
def help_usage():
	print'{0:s} [-h|--help] -u|--audio file_dsp_in -d|--dsp file_dsp_out -a|--accept -i|--input -o|--output -s|--server serwername -p|--port portnum -l|--logfile logfn -1-5 -b|--blocksize block -x -c|--rawin -e|--rawout -f|--rawall -y|--full --afmt num --achn num --aspd num --mixrec 0-100 --mixmic 0-100 --mixvol 0-100 --mixpcm 0-100 -n|--mix -g|--debug -L|--low -H|--high|--ogg -O|--sox -S|--spm -M| --soxin -X SoXBuffSeconds| --zombie -Z | --chf cutOggFrom | --cht cutOggTo | --defogg | --stdogg| --speex -P | --speexq -Q speexQual 0-10 | --oggq -1-10 | --lowspeex | --stdspeex| | --highspeex | --zspx'.format(sys.argv[0])
	print'Default server is localhost'
	print'Default port is 9000'
	print'Acceptor(server) mode is -a'
	print'Log file name with -l, level -1-5'
	print'Input client is recording and playing on server (default)'
	print'Output client is playing recieved from server'
	print'Block size set -b, --blocksize'
	print'Bidirection mode set off -x, use -i or -o to select mode'
	print'Raw audio input device set -u, --audio_in'
	print'Raw audio output device set -d, --audio_out'
	print'Raw input mode set -c, --rawin'
	print'Raw output mode set -e, --rawout'
	print'Raw all mode set -f, --rawall or -c -e'
	print'Uncompressed mode set -y, --full'
	print'In OSS mode (default), audio formats: --afmt, --achn, --aspd'
	print'Audio format --afmt: 1=MULAW, 2=ALAW, 4=IMADPCM, 8=U8, 16=S16LE, 32=S16BE, 64=S8, 128=U16LE, 256=U16BE'
	print'Audio channels --achn: 1, 2, ?'
	print'Audio speed --aspd: 8000, 11025, 22050, 44100, 96000'
	print'Audio defaults: fmt=MULAW, chn=1, spd=8000'
	print'Mixer input --mixrec --mixmic in range 0-100, default 90, 50'
	print'Mixer output --mixvol --mixpcm in range 0-100, default 40, 40'
	print'Use mixer settings -m, --mix'
	print'Debug write in/out files use -g, --debug'
	print'Lowest possible quality (deformed voice) --low, -L'
	print'Quite high quality --high, -H'
	print'CD stereo quality --cd'
	print'Use ogg encoding/decoding --ogg, -O'
	print'Cut header from/to --chf --cht'
	print'Ogg defaults --defogg, ogg quality --oggq, standard ogg --stdogg'
	print'Use SoX for sound I/O --sox, -S, buffer in seconds --soxin -X'
	print'Use SPM -Mechanik- settings --spm, -M'
	print'Zombie mode - hardcore sound, big lag, minimum of minimum transfer --zombie -Z'
	print'Speex enable --speex -P, speex quality --speexq -Q 0-10'
	print'Speex defaults --defspeex, speex good mode --highspeex, low mode --lowspeex'
	print'Speex max compression, biggest buffer, BZ2, LAG, minimum BPS possible at all --zspx'

if __name__ == '__main__':
    main()
