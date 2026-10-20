// Exercise startup executable discovery before its first call from main:
// relative argv[0] and PATH-based launches must survive a changed environment.
#include "linux/process.h"
#include "test_util.h"

#include <cstdlib>
#include <limits.h>
#include <string>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    char resolved[PATH_MAX];
    if (!::realpath(argv[1], resolved))
        return 2;
    const std::string expected = resolved;
    plat_test::runCase("executable path survives changes to cwd and PATH", [&] {
        CHECK(std::string(argv[0]) == argv[2]);
        CHECK(::chdir("/") == 0);
        CHECK(::setenv("PATH", "/nonexistent/plat-test-bin", 1) == 0);
        const std::string path = plat::linux_process::executablePath();
        CHECK(!path.empty() && path.front() == '/');
        CHECK(path == expected);
        CHECK(::access(path.c_str(), X_OK) == 0);
        CHECK(plat::linux_process::executablePath() == path);
    });
    return plat_test::summary();
}
