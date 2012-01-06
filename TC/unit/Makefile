CC ?= gcc

TARGETS = utc_ApplicationFW_alarmmgr_init_func \
	  utc_ApplicationFW_alarmmgr_set_cb_func \
	  utc_ApplicationFW_alarmmgr_create_alarm_func \
	  utc_ApplicationFW_alarmmgr_free_alarm_func \
	  utc_ApplicationFW_alarmmgr_set_time_func \
	  utc_ApplicationFW_alarmmgr_get_time_func \
  	  utc_ApplicationFW_alarmmgr_set_repeat_mode_func \
	  utc_ApplicationFW_alarmmgr_get_repeat_mode_func \
	  utc_ApplicationFW_alarmmgr_set_type_func \
	  utc_ApplicationFW_alarmmgr_get_type_func \
	  utc_ApplicationFW_alarmmgr_add_alarm_with_localtime_func \
	  utc_ApplicationFW_alarmmgr_add_alarm_func \
	  utc_ApplicationFW_alarmmgr_remove_alarm_func \
	  utc_ApplicationFW_alarmmgr_enum_alarm_ids_func \
	  utc_ApplicationFW_alarmmgr_get_info_func

PKGS = alarm-service 

LDFLAGS = `pkg-config --libs $(PKGS)`
LDFLAGS += $(TET_ROOT)/lib/tet3/tcm_s.o
LDFLAGS += -L$(TET_ROOT)/lib/tet3 -ltcm_s
LDFLAGS += -L$(TET_ROOT)/lib/tet3 -lapi_s

CFLAGS = -I. `pkg-config --cflags $(PKGS)`
CFLAGS += -I$(TET_ROOT)/inc/tet3
CFLAGS += -Wall

all: $(TARGETS)

$(TARGETS): %: %.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGETS)
