#include <string.h>
#include <stdio.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

void testFM_setup(void **state);
void testFM_teardown(void **state);

void testFM_init(void **state);
void testFM_monitor(void **state);
void testFM_monitorNonExistent(void **state);
void testFM_monitorReentrant(void **state);
void testFM_monitorTooMany(void **state);

void testFM_unMonitor(void **state);
void testFM_unMonitorNotMonitored(void **state);

void testFM_onWatchSetup(void **state);
void testFM_onUpdate(void **state);
void testFM_onUpdate3Files(void **state);
void testFM_onDelete(void **state);

void testFM_nonExistingPathsTrue(void **state);
void testFM_nonExistingPathsFalse(void **state);
void testFM_nonExistingPathsFalse2(void **state);

void testFM_detectFileCreation(void **state);
void testFM_detectFileCreatedMulti(void **state);

void testFM_addMonitorOnDelete(void **state);

void testFM_nonblocking(void **state);

int main(int argc, char* argv[]) {
        const UnitTest tests[] = {

                unit_test_setup_teardown(testFM_init,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_monitor,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_monitorNonExistent,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_monitorReentrant,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_monitorTooMany,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_unMonitor,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_unMonitorNotMonitored,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_onWatchSetup,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_onUpdate,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_onUpdate3Files,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_onDelete,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_nonExistingPathsTrue,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_nonExistingPathsFalse,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_nonExistingPathsFalse2,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_detectFileCreation,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_detectFileCreatedMulti,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_addMonitorOnDelete,
                                         testFM_setup,
                                         testFM_teardown),

                unit_test_setup_teardown(testFM_nonblocking,
                                         testFM_setup,
                                         testFM_teardown),

        };

        if(argc > 1) {
                int i;
                const int numTests = sizeof(tests) / sizeof(tests[0]);

                if(argv[1][0] == '?') {
                        // The sequence in tests is (test1_setup, test1, test1_teardown,
                        // ...). So every third object, starting from 1, is the actual test.
                        for(i = 1; i < numTests; i += 3) {
                                printf("Test %d: %s\n", (i - 1) / 3, tests[i].name);
                        }

                        return 0;
                }

                for(i = 1; i < numTests; i += 3) {
                        if(strcmp(argv[1], tests[i].name) == 0) {
                                const UnitTest singleTest[] = {
                                        tests[i - 1], tests[i], tests[i + 1]
                                };

                                return run_tests(singleTest);
                        }
                }

                print_error("Could not find test '%s'\n", argv[1]);
                fail();
        }

        return run_tests(tests);
}
