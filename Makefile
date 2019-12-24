all: web_proxy

web_proxy: web_proxy.o
	g++ -o web_proxy web_proxy.o -lpthread

web_proxy.o: web_proxy.cpp
	g++ -c -o web_proxy.o web_proxy.cpp 

clean:
	rm -f *.o web_proxy 