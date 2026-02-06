gcc -O3 -c Deserialize.c -I./ -I./ini -I./llhttp -I./hp-socket-6.0.4-lib-linux/include/hpsocket
gcc -O3 -c sha1.c -I./ -I./ini -I./llhttp -I./hp-socket-6.0.4-lib-linux/include/hpsocket
gcc -O3 -c ini/dictionary.c -I./ -I./ini -I./llhttp -I./hp-socket-6.0.4-lib-linux/include/hpsocket
gcc -O3 -c ini/iniparser.c -I./ -I./ini -I./llhttp -I./hp-socket-6.0.4-lib-linux/include/hpsocket
gcc -O3 -c llhttp/api.c -I./ -I./ini -I./llhttp -I./hp-socket-6.0.4-lib-linux/include/hpsocket
gcc -O3 -c llhttp/llhttp.c -I./ -I./ini -I./llhttp -I./hp-socket-6.0.4-lib-linux/include/hpsocket
gcc -O3 -c llhttp/http.c -I./ -I./ini -I./llhttp -I./hp-socket-6.0.4-lib-linux/include/hpsocket
g++ -O3 -c main.cpp -I./ -I./ini -I./llhttp -I./hp-socket-6.0.4-lib-linux/include/hpsocket

g++ Deserialize.o sha1.o dictionary.o iniparser.o api.o llhttp.o http.o main.o -o signalserver -s -L./hp-socket-6.0.4-lib-linux/lib/hpsocket4c/x64 -luuid -lpthread -lhpsocket4c
