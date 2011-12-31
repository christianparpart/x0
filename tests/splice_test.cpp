#include <x0/io/Pipe.h>
#include <x0/io/PipeSink.h>
#include <x0/io/FileSource.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_pipe2pipe()
{
	const char* s = "Hello, World";
	ssize_t slen = strlen(s);

	x0::Pipe a, b;

	a.write(s, slen);
	a.read(&b, a.size());
	//b.write(&a, a.size());

	char buf[1024];
	ssize_t n = b.read(buf, sizeof(buf));

	assert(slen == n);
	assert(memcmp(s, buf, n) == 0);
}

void test_file2pipe()
{
	x0::FileSource in("/etc/passwd");
	x0::Pipe pipe;
	x0::PipeSink out(&pipe);

	ssize_t rv = in.sendto(out);
	printf("in.sendto: %ld\n", rv);
	printf("pipe.size: %ld\n", pipe.size());

	assert((size_t)rv == pipe.size());

	char buf[16 * 1024];
	rv = pipe.read(buf, sizeof(buf));
//	::write(1, buf, rv);
}

int main(int argc, char* argv[])
{
//	test_pipe2pipe();
	test_file2pipe();

	return 0;
}
