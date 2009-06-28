#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TextTestRunner.h>
#include <sysexits.h>

int main(int argc, const char *argv[])
{
	CppUnit::TextTestRunner runner;
	runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());

	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
