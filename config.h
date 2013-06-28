/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* The archive API is available */
/* #undef ENABLE_ARCHIVE */

/* Define to enable libcdio_paranoia support */
/* #undef ENABLE_CDIO_PARANOIA */

/* Define when libcurl is used for HTTP streaming */
#define ENABLE_CURL 1

/* Define when despotify is enabled */
/* #undef ENABLE_DESPOTIFY */

/* Define to enable the encoder plugins */
/* #undef ENABLE_ENCODER */

/* Define to enable the libffado output plugin */
/* #undef ENABLE_FFADO_OUTPUT */

/* Define to enable the FLAC encoder plugin */
/* #undef ENABLE_FLAC_ENCODER */

/* Define for fluidsynth support */
/* #undef ENABLE_FLUIDSYNTH */

/* Define to enable the HTTP server output */
/* #undef ENABLE_HTTPD_OUTPUT */

/* Define to enable inotify support */
#define ENABLE_INOTIFY 1

/* Define to enable the lame encoder plugin */
/* #undef ENABLE_LAME_ENCODER */

/* Define if large file support is enabled */
#define ENABLE_LARGEFILE 1

/* Define when last.fm radio is enabled */
/* #undef ENABLE_LASTFM */

/* Define for mikmod support */
/* #undef ENABLE_MIKMOD_DECODER */

/* Define when libmms is used for the MMS protocol */
/* #undef ENABLE_MMS */

/* Define to enable support for writing audio to a pipe */
/* #undef ENABLE_PIPE_OUTPUT */

/* Define to enable the recorder output */
/* #undef ENABLE_RECORDER_OUTPUT */

/* Define for libsidplay2 support */
/* #undef ENABLE_SIDPLAY */

/* Define to enable the sndfile decoder plugin */
/* #undef ENABLE_SNDFILE */

/* Define to enable Solaris /dev/audio support */
/* #undef ENABLE_SOLARIS_OUTPUT */

/* Define when soundcloud is enabled */
#define ENABLE_SOUNDCLOUD 1

/* Define when libsoup is used for HTTP streaming */
/* #undef ENABLE_SOUP */

/* Define to enable sqlite database support */
#define ENABLE_SQLITE 1

/* Define to use the systemd daemon library */
/* #undef ENABLE_SYSTEMD_DAEMON */

/* Define to enable the TwoLAME encoder plugin */
/* #undef ENABLE_TWOLAME_ENCODER */

/* Define for Ogg Vorbis support */
/* #undef ENABLE_VORBIS_DECODER */

/* Define to enable the vorbis encoder plugin */
/* #undef ENABLE_VORBIS_ENCODER */

/* Define to enable the PCM wave encoder plugin */
/* #undef ENABLE_WAVE_ENCODER */

/* Define for wildmidi support */
/* #undef ENABLE_WILDMIDI */

/* Define to enable WinMM support */
/* #undef ENABLE_WINMM_OUTPUT */

/* Define to 1 if you have the `accept4' function. */
/* #undef HAVE_ACCEPT4 */

/* Define to enable ALSA support */
/* #undef #define HAVE_ALSA */

/* Define to play with ao */
/* #undef HAVE_AO */

/* Define for audiofile support */
/* #undef HAVE_AUDIOFILE */

/* Define to enable Avahi Zeroconf support */
/* #undef HAVE_AVAHI */

/* Define to enable Bonjour Zeroconf support */
/* #undef HAVE_BONJOUR */

/* Define to have bz2 archive support */
/* #undef HAVE_BZ2 */

/* Define to 1 if you have the `daemon' function. */
#define HAVE_DAEMON 1

/* Define to 1 if `dontUpSampleImplicitSBR' is a member of
   `faacDecConfiguration'. */
/* #undef HAVE_FAACDECCONFIGURATION_DONTUPSAMPLEIMPLICITSBR */

/* Define to 1 if `downMatrix' is a member of `faacDecConfiguration'. */
/* #undef HAVE_FAACDECCONFIGURATION_DOWNMATRIX */

/* Define to 1 if `samplerate' is a member of `faacDecFrameInfo'. */
/* #undef HAVE_FAACDECFRAMEINFO_SAMPLERATE */

/* Define to use FAAD2 for AAC decoding */
/* #undef HAVE_FAAD */

/* Define if FAAD2 uses buflen in function calls */
/* #undef HAVE_FAAD_BUFLEN_FUNCS */

/* Define if faad.h uses the broken "unsigned long" pointers */
/* #undef HAVE_FAAD_LONG */

/* Define for FFMPEG support */
#define HAVE_FFMPEG 1

/* Define to enable support for writing audio to a FIFO */
/* #undef HAVE_FIFO */

/* Define for FLAC support */
/* #undef HAVE_FLAC */

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the `getpeereid' function. */
/* #undef HAVE_GETPEEREID */

/* Define for gme support */
/* #undef HAVE_GME */

/* Define to use id3tag */
/* #undef HAVE_ID3TAG */

/* Define to 1 if you have the `inotify_init' function. */
#define HAVE_INOTIFY_INIT 1

/* Define to 1 if you have the `inotify_init1' function. */
/* #undef HAVE_INOTIFY_INIT1 */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if IPv6 support present */
/* #undef HAVE_IPV6 */

/* Define to have ISO9660 archive support */
/* #undef HAVE_ISO9660 */

/* Define to enable JACK support */
/* #undef HAVE_JACK */

/* Define to 1 if you have the `jack_set_info_function' function. */
/* #undef HAVE_JACK_SET_INFO_FUNCTION */

/* Define to 1 if you have lame 3.98 or greater. */
/* #undef HAVE_LAME */

/* Define to 1 if you have the <lame.h> header file. */
/* #undef HAVE_LAME_H */

/* Define to 1 if you have the <lame/lame.h> header file. */
/* #undef HAVE_LAME_LAME_H */

/* Define to 1 if you have the `dns_sd' library (-ldns_sd). */
/* #undef HAVE_LIBDNS_SD */

/* Define to 1 if you have the `faad' library (-lfaad). */
/* #undef HAVE_LIBFAAD */

/* Define to 1 if you have the `mp3lame' library (-lmp3lame). */
/* #undef HAVE_LIBMP3LAME */

/* Define to 1 if you have the `mp4ff' library (-lmp4ff). */
/* #undef HAVE_LIBMP4FF */

/* Define to enable libsamplerate */
/* #undef HAVE_LIBSAMPLERATE */

/* libsamplerate doesn't provide src_int_to_float_array() (<0.1.3) */
/* #undef HAVE_LIBSAMPLERATE_NOINT */

/* Define to 1 if you have the `sidutils' library (-lsidutils). */
/* #undef HAVE_LIBSIDUTILS */

/* Define to 1 if you have the `vorbisidec' library (-lvorbisidec). */
/* #undef HAVE_LIBVORBISIDEC */

/* Define to 1 if you have the `WildMidi' library (-lWildMidi). */
/* #undef HAVE_LIBWILDMIDI */

/* define to enable libwrap library */
/* #undef HAVE_LIBWRAP */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to use libmad */
/* #undef HAVE_MAD */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define for modplug support */
/* #undef HAVE_MODPLUG */

/* Define to use FAAD2+mp4ff for MP4 decoding */
/* #undef HAVE_MP4 */

/* Define to use libmpcdec for MPC decoding */
/* #undef HAVE_MPCDEC */

/* Define to use libmpg123 */
/* #undef HAVE_MPG123 */

/* Define to enable Hauppauge Media MVP support */
/* #undef HAVE_MVP */

/* Define for OpenAL support */
/* #undef HAVE_OPENAL */

/* Define to 1 if you have the <OpenAL/alc.h> header file. */
/* #undef HAVE_OPENAL_ALC_H */

/* Define to 1 if you have the <OpenAL/al.h> header file. */
/* #undef HAVE_OPENAL_AL_H */

/* Define to enable OSS */
/* #undef HAVE_OSS */

/* Define for compiling OS X support */
/* #undef HAVE_OSX */

/* Define to 1 if you have the `pipe2' function. */
#define HAVE_PIPE2 1

/* Define to enable PulseAudio support */
/* #undef HAVE_PULSE 1 */

/* Define to enable ROAR support */
/* #undef HAVE_ROAR */

/* Define to enable the shoutcast output */
/* #undef HAVE_SHOUT */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define if struct ucred is present from sys/socket.h */
#define HAVE_STRUCT_UCRED 1

/* Define if syslog() is available */
/* #undef HAVE_SYSLOG */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if TCP socket support is enabled */
#define HAVE_TCP 1

/* Define to 1 if you have the <tcpd.h> header file. */
/* #undef HAVE_TCPD_H */

/* Define to use tremor (libvorbisidec) for ogg support */
/* #undef HAVE_TREMOR */

/* Define if unix domain socket support is enabled */
#define HAVE_UN 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <valgrind/memcheck.h> header file. */
/* #undef HAVE_VALGRIND_MEMCHECK_H */

/* Define to enable WavPack support */
/* #undef HAVE_WAVPACK */

/* Defined if WildMidi_SampledSeek() is available (libwildmidi <= 0.2.2) */
/* #undef HAVE_WILDMIDI_SAMPLED_SEEK */

/* Define to enable Zeroconf support */
/* #undef HAVE_ZEROCONF */

/* Define to have zip archive support */
/* #undef HAVE_ZZIP */

/* Define if an old pre-SV8 libmpcdec is used */
/* #undef MPC_IS_OLD_API */

/* Name of package */
#define PACKAGE "mpd"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "musicpd-dev-team@lists.sourceforge.net"

/* Define to the full name of this package. */
#define PACKAGE_NAME "mpd"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "mpd 0.17.4"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "mpd"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.17.4"

/* The MPD protocol version */
#define PROTOCOL_VERSION "0.17.0"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.17.4"

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

#define ENABLE_ANDROID_OUTPUT 1
#define HAVE_ANDROID_LOG 1
