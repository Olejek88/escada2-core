kernel: kernel.o
	cd version; make; ./version
	cd ..
	g++ kernel.o logs.o dbase.o tinyxml2.o -o kernel -lpthread -lmysqlclient -L/usr/lib -lmysqlclient -lmodbus -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/libgtop-2.0 -lgtop-2.0 -lglib-2.0
kernel.o: kernel.cpp logs.cpp
	g++ -c logs.cpp
	g++ -c kernel.cpp
	g++ -c tinyxml2.cpp
	g++ -c dbase.cpp
