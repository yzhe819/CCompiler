test: compile
	./cc test.c

bootstrap: compile
	./cc cc.c test.c
  
compile:
	gcc -m32 ./cc.c -o cc