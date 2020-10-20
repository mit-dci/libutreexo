CXX      = c++
AR       = ar
ARFLAGS  = -cr
CXXFLAGS = -O2 -g -Wall -Wextra
CPPFLAGS = 
LDFLAGS  =

LIBUTREEXO = libutreexo.a
LIBUTREEXO_OBJS  =
LIBUTREEXO_OBJS += state.o
LIBUTREEXO_OBJS += uint256.o
LIBUTREEXO_OBJS += utreexo.o
LIBUTREEXO_OBJS += util/strencodings.o
LIBUTREEXO_OBJS += crypto/sha256.o
LIBUTREEXO_OBJS += crypto/sha256_avx2.o
LIBUTREEXO_OBJS += crypto/sha256_shani.o
LIBUTREEXO_OBJS += crypto/sha256_sse4.o
LIBUTREEXO_OBJS += crypto/sha256_sse41.o

TEST_OBJS =
TEST_OBJS += state_tests.o
TEST_OBJS += tests.o
TEST_UTREEXO = test_utreexo

OBJS   =
OBJS  += $(LIBUTREEXO_OBJS)
OBJS  += $(TEST_OBJS)

LIBS   =
LIBS  += $(LIBUTREEXO)

PROGS  =
PROGS += $(TEST_UTREEXO)

CXXFLAGS_INT = -std=c++11
CPPFLAGS_INT = -I.
LDFLAGS_INT  = -lboost_unit_test_framework

SSE41_CXXFLAGS = -msse4.1
AVX2_CXXFLAGS = -mavx -mavx2
SHANI_CXXFLAGS = -msse4 -msha

crypto/sha256_avx2.o: CXXFLAGS_INT += $(AVX2_CXXFLAGS)
crypto/sha256_avx2.o: CPPFLAGS_INT += -DENABLE_AVX2

crypto/sha256_shani.o: CXXFLAGS_INT += $(SHANI_CXXFLAGS)
crypto/sha256_shani.o: CPPFLAGS_INT += -DENABLE_SHANI

crypto/sha256_sse41.o: CXXFLAGS_INT += $(SSE41_CXXFLAGS)
crypto/sha256_sse41.o: CPPFLAGS_INT += -DENABLE_SSE41

crypto/sha256.o: CPPFLAGS_INT += -DENABLE_AVX2 -DENABLE_SHANI -DENABLE_SSE41 -DUSE_ASM

V=
_notat_=@
_notat_0=$(_notat_)
_notat_1=@\#
_at_=@
_at_0=$(_at_)
_at_1=
at = $(_at_$(V))
notat = $(_notat_$(V))

DEPDIR=.deps
DEPS = $(addprefix $(DEPDIR)/,$(OBJS:.o=.o.Tpo))
DIRS = $(dir $(DEPS))
DEPDIRSTAMP=$(DEPDIR)/.stamp

all: $(PROGS) $(LIBS)

-include $(DEPS)

$(DEPDIRSTAMP):
	@mkdir -p $(dir $(DEPS))
	@touch $@

$(OBJS): | $(DEPDIRSTAMP)

%.o: %.cpp
	$(notat)echo CXX $<
	$(at)$(CXX) $(CPPFLAGS_INT) $(CPPFLAGS) $(CXXFLAGS_INT) $(CXXFLAGS) $(CXXFLAGS_EXTRA) -c -MMD -MP -MF .deps/$@.Tpo $< -o $@

crypto/%.o: crypto/%.cpp
	$(notat)echo CXX $<
	$(at)$(CXX) $(CPPFLAGS_INT) $(CPPFLAGS) $(CXXFLAGS_INT) $(CXXFLAGS) -c -MMD -MP -MF .deps/$@.Tpo $< -o $@

$(LIBUTREEXO): $(LIBUTREEXO_OBJS)
	$(notat)echo AR $@
	$(at)$(AR) $(ARFLAGS) $@ $^

$(TEST_UTREEXO): $(TEST_OBJS) $(LIBUTREEXO)
	$(notat)echo LINK $@
	$(at)$(CXX) $(CXXFLAGS_INT) $(CXXFLAGS) $(LDFLAGS_INT) $(LDFLAGS) $^ -o $@

clean:
	-rm -f $(PROGS)
	-rm -f $(LIBS)
	-rm -f $(OBJS)
	-rm -f $(DEPS)
	-rm -rf $(DEPDIR)
