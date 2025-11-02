

DIRS = h20_hs \
	data_rec \
	# h20	\
	# encrypt \
	# jd100 \
	
.PHONY: all clean

all:
	$(call make_sub)

clean:
	$(call make_sub, clean)


define make_sub
    @for i in $(DIRS); do \
	make -C $$i $1;\
	done
endef
