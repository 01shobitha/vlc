# Darwin rules
all: .autoconf .automake .libtool .intl .pkgcfg .freetype .fribidi \
    .a52 .mpeg2 .id3tag .mad .ogg .vorbis .vorbisenc .theora \
    .flac .speex .shout .faad .faac .lame .twolame .ebml .matroska .ffmpeg \
    .dvdcss .dvdnav .dvdread .dvbpsi .live .caca .mod \
    .png .gpg-error .gcrypt .gnutls .opendaap .cddb .cdio .vcdimager \
    .SDL_image .glib .gecko .mpcdec .dirac_encoder .dirac_decoder \
    .libdca .tag .x264 .goom2k4 .aclocal
# .expat .clinkcc don't work with SDK yet
# .glib .libidl .gecko are required to build the mozilla plugin
# .mozilla will build an entire mozilla. it can be used if we need to create a new .gecko package

