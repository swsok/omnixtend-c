all:
	gcc -g -o tloe_endpoint_stop_and_wait tloe_endpoint_stop_and_wait.c tloe_ether.c
	gcc -g -o tloe_endpoint_stop_and_wait_slave tloe_endpoint_stop_and_wait_slave.c tloe_frame.c tloe_ether.c
	gcc -g -o tloe_endpoint_sliding_window tloe_endpoint_sliding_window.c tloe_ether.c tloe_frame.c util/circular_queue.c 
	gcc -g -o tloe_endpoint_sliding_window_slave tloe_endpoint_sliding_window_slave.c tloe_ether.c tloe_frame.c util/circular_queue.c 

rmmsgq:
	rm /dev/mqueue/tloe_ether-a /dev/mqueue/tloe_ether-b

clean:
	rm -rf *.o tloe_endpoint_stop_and_wait tloe_endpoint_stop_and_wait_slave tloe_endpoint_sliding_window tloe_endpoint_sliding_window_slave
