# ---------------------------------------------------------------------
  # System 
# ---------------------------------------------------------------------

  SYSTEM     = x86-64_linux
  LIBFORMAT  = static_pic

  # ---------------------------------------------------------------------
# Compiler selection 
 # ---------------------------------------------------------------------

 CCC       = g++ -O0
 EXECNAME  = execfile

 # ---------------------------------------------------------------------
 # Compiler options 
 # ---------------------------------------------------------------------

 CCOPT = -m64 -O -fPIC -fno-strict-aliasing -fexceptions -DNDEBUG -DIL_STD
 COPT  = -m64 -fPIC -fno-strict-aliasing
 JOPT  = -classpath $(CPLEXDIR)/lib/cplex.jar -O

 # ---------------------------------------------------------------------
 # CPLEX
 # ---------------------------------------------------------------------

 NOT_ICEX := $(firstword $(wildcard /mnt/nfs/modules/apps/cplex/12.6/cplex))

ifeq (,$(NOT_ICEX))
	CPLEXDIR   = /opt/ibm/ILOG/CPLEX_Studio126/cplex
	CONCERTDIR = /opt/ibm/ILOG/CPLEX_Studio126/concert
else
	CPLEXDIR   = /mnt/nfs/modules/apps/cplex/12.6/cplex
	CONCERTDIR = /mnt/nfs/modules/apps/cplex/12.6/concert
endif

                                 # ---------------------------------------------------------------------
# Link options and libraries
 # ---------------------------------------------------------------------

 CPLEXBINDIR   = $(CPLEXDIR)/bin/$(BINDIST)
 CPLEXLIBDIR   = $(CPLEXDIR)/lib/$(SYSTEM)/$(LIBFORMAT)
 CONCERTLIBDIR = $(CONCERTDIR)/lib/$(SYSTEM)/$(LIBFORMAT)

 CONCERTINCDIR = $(CONCERTDIR)/include
 CPLEXINCDIR   = $(CPLEXDIR)/include

 CPLEXFLAGS = -L$(CPLEXLIBDIR) -L$(CONCERTLIBDIR) -lconcert -lilocplex -lcplex -lm -lpthread

 CFLAGS    = -c -Wall $(CCOPT) -I$(CPLEXINCDIR) -I$(CONCERTINCDIR) 
 LDFLAGS   = $(CCOPT) -I$(CPLEXINCDIR) -I$(CONCERTINCDIR)
 LNFLAGS   = $(CPLEXFLAGS) -lrt

 INCLUDE   = -I./include 
 SRCDIR    = src/ 
 SRCLIST   = $(shell find $(SRCDIR) -name '*.cpp')
 OBJDIR    = obj/
 OBJLIST   = $(patsubst $(SRCDIR)%.cpp, $(OBJDIR)%.o, $(SRCLIST))

 # ---------------------------------------------------------------------
 # COMPILATION
 # ---------------------------------------------------------------------
 all:$(EXECNAME)

$(EXECNAME): $(OBJLIST) 
	$(CCC) $(LDFLAGS) $(OBJLIST) -o $@ $(LNFLAGS)

$(OBJLIST): $(OBJDIR)%.o: $(SRCDIR)%.cpp
	$(CCC) $(CFLAGS) $? -o $@ $(INCLUDE)

clean:
	rm -rf $(OBJDIR)*.o

