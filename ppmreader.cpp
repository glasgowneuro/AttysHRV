#include <stdio.h>

int main(int, char**) {
	FILE* f=fopen("front.ppm","rb");
	int width, height, max_colour;
	fscanf (f, "P6 %d %d %d", &width, &height, &max_colour);
	unsigned char c;
	fscanf (f, "%c",&c);
	printf("%d %d %d\n%x\n", width, height, max_colour,c);
	fclose(f);
}
