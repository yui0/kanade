CC	= gcc
#CFLAGS	= -Os -Wall -I. -I./parser
#CFLAGS	= -Os -I. -I./parser
CFLAGS	= -Os -I.
DEST	= /usr/local/bin
LDFLAGS	= -L/usr/local/lib
LIBS	= -lm
OBJS	= $(patsubst %.c,%.o,$(wildcard *.c))
#OBJS	:= $(OBJS) $(patsubst %.c,%.o,$(wildcard voice/*.cpp))
#OBJS	:= $(OBJS) $(patsubst %.c,%.o,$(wildcard parser/*.c))
#OBJS	:= $(OBJS) $(patsubst %.c,%.o,$(wildcard PSOLA/*.cpp))
#OBJS	:= $(OBJS) $(patsubst %.c,%.o,$(wildcard score/*.c))
PROGRAM	= kanade

all:		$(PROGRAM)

$(PROGRAM):	$(OBJS)
		$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) $(LIBS) -o $(PROGRAM)

clean:;		rm -f *.o *~ $(PROGRAM)

install:	$(PROGRAM)
		install -s $(PROGRAM) $(DEST)
