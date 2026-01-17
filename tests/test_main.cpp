#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TextOutputter.h>
#include <cppunit/CompilerOutputter.h>

int main() {
    // Get the top level suite from the registry
    CppUnit::Test *suite = CppUnit::TestFactoryRegistry::getRegistry().makeTest();

    // Create the test runner
    CppUnit::TextUi::TestRunner runner;
    runner.addTest(suite);

    // Run the tests
    bool success = runner.run();

    // Return 0 if all tests passed, 1 if any failed
    return success ? 0 : 1;
}
