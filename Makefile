#
# Makefile
#
NAME = scci

ifeq ($(OSX),1)
WIN32=0 
else
WIN32=1
endif

ifeq ($(WIN32),1)
include Makefile.w32_cross
else
include Makefile.unix
endif

#
# common
#

USE_OPL3 = 1
USE_OPM = 1
USE_FMGEN = 1

#
#
#

LIBNAME = librend.a

#
# directories
#

MAINDIR   = .

LIBDIR = aslplug/src

FMTDIR    = format
DEVDIR    = device
NESDIR    = device/nes
OPLDIR    = device/opl

GLUEDIR = aslplug/glue
LOGDIR = aslplug/log

OTHERDIR = aslplug/other
DRVW32DIR = $(OTHERDIR)/snddrv/win32
DRVDIR = $(OTHERDIR)/snddrv

FMTSRCS = \
 audiosys.c handler.c songinfo.c 

DEVSRCS = \
 s_logtbl.c s_psg.c
  
OPLSRCS = \
 s_deltat.c s_opl.c s_opltbl.c

MAINSRCS = \
 sccisim.cpp render.c io_dummy.c
 
LOGSRCS = \
 nlg.c s98x.c log.c

DRVSRCS = \
 snddrv.c

DRVW32SRCS = \
 snddrvwo.c snddrvds.c

LIBDIRS = 
LIBDIRS += $(FMTDIR) 
LIBDIRS += $(DEVDIR) $(NESDIR) $(OPLDIR)

DIRS = $(OTHERDIR) 
DIRS += $(LOGDIR) $(DRVDIR) $(DRVW32DIR)
DIRS += $(GLUEDIR)

LIBSRCS = \
       $(addprefix $(FMTDIR)/,$(FMTSRCS)) \
       $(addprefix $(DEVDIR)/,$(DEVSRCS)) \
       $(addprefix $(NESDIR)/,$(NESSRCS)) \
       $(addprefix $(OPLDIR)/,$(OPLSRCS)) \


SRCS = $(addprefix $(LOGDIR)/,$(LOGSRCS)) \
       $(addprefix $(DRVDIR)/,$(DRVSRCS)) \
       $(addprefix $(DRVW32DIR)/,$(DRVW32SRCS)) \
       $(addprefix $(MAINDIR)/,$(MAINSRCS)) \

#### GMCDRV

CFLAGS += -DUSE_GMCDRV -DDLLMAKE

GMCDIR = aslplug/gmcdrv

ifeq ($(WIN32),1)
GMCSRCS = \
  gmcdrv.cpp
else
GMCSRCS = \
  gmcdrv_dummy.cpp
endif

DIRS += $(GMCDIR)
SRCS += $(addprefix $(GMCDIR)/,$(GMCSRCS)) 

####  OPL3

ifeq ($(USE_OPL3),1)

DEVSRCS += s_opl4.c
CFLAGS += -DUSE_OPL3
OPL3DIR = device/mame
OPL3SRCS = ymf262.c
LIBDIRS += $(OPL3DIR)
LIBSRCS += $(addprefix $(OPL3DIR)/,$(OPL3SRCS))

endif

#### OPM

ifeq ($(USE_OPM),1)

DEVSRCS += s_opm.c
CFLAGS += -DUSE_OPM
OPMDIR = device/ym2151
OPMSRCS = ym2151.c
LIBDIRS += $(OPMDIR)
LIBSRCS += $(addprefix $(OPMDIR)/,$(OPMSRCS))

endif

#### FMGEN

ifeq ($(USE_FMGEN),1)

DEVSRCS += s_opm_gen.cpp s_opna_gen.cpp
CFLAGS += -DUSE_FMGEN
GENDIR = device/fmgen
GENSRCS = fmgen.cpp fmtimer.cpp opm.cpp opna.cpp psg.cpp file.cpp
LIBDIRS += $(GENDIR)
LIBSRCS += $(addprefix $(GENDIR)/,$(GENSRCS))

endif

#
#
#

# $(info source $(SRCS))
# $(info objs $(OBJS))


LIBOBJDIR = $(OBJDIR)/$(LIBDIR)



LIBOBJS = $(addprefix $(LIBOBJDIR)/,$(addsuffix .o,$(basename $(LIBSRCS))))
OBJS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(SRCS))))

LIBARC := $(OBJDIR)/$(LIBNAME)

CFLAGS += -I$(LIBDIR)
CFLAGS += $(addprefix -I$(LIBDIR)/,$(LIBDIRS))
CFLAGS += $(addprefix -I,$(DIRS))
CFLAGS += -I$(SRCDIR)
CFLAGS += -I. 


ifdef DEBUG
CFLAGS += -O0 -g
else
CFLAGS += -O3
endif

DTESTOBJS = $(OBJDIR)/dtest.o

#
#
#

dynamic : $(TARGET)

static : $(TARGET_S)

test : $(TEST)

lib : $(LIBARC)

#
# Rules
#

$(OBJDIR)/%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : %.cpp
	$(CXX) -c $(CFLAGS) -o $@ $<

$(LIBARC) : $(OBJDIR) $(LIBOBJS)
	$(AR) rcs $@ $(LIBOBJS)

$(OBJDIR) :
	mkdir -p $(OBJDIR)
	mkdir -p $(addprefix $(OBJDIR)/$(LIBDIR)/,$(LIBDIRS))
	mkdir -p $(addprefix $(OBJDIR)/,$(DIRS))

#
# target
#

$(TARGET) : $(OBJDIR) $(LIBARC) $(OBJS) $(DLLDEF)
	$(CXX) -static -shared $(OBJS) $(LIBARC) -o $@ -lwinmm -ldsound -ldxguid


$(TEST) : $(TARGET) $(DTESTOBJS)
	$(CXX) $(CFLAGS) -mconsole -o $(TEST) $(DTESTOBJS) $(OBJS) $(LIBARC)  -lwinmm -ldsound -ldxguid

clean :
	rm -rf $(OBJDIR)
	rm -f $(TEST)
	rm -f $(TARGET)
	rm -f $(DLLDEF)
