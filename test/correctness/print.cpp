#include <stdio.h>
#include <string>
#include <vector>
#include <limits>
#include <Halide.h>

using namespace Halide;

std::vector<std::string> messages;

extern "C" void halide_print(void *user_context, const char *message) {
    //printf("%s", message);
    messages.push_back(message);
}


int main(int argc, char **argv) {
    #ifdef _WIN32
    printf("Skipping test because use of varags on windows under older llvms (e.g. pnacl) crashes.");
    return 0;
    #endif

    Var x;

    {
        Func f;

        f(x) = print(x * x, "the answer is", 42.0f, "unsigned", cast<uint32_t>(145));
        f.set_custom_print(halide_print);
        Image<int32_t> result = f.realize(10);

        for (int32_t i = 0; i < 10; i++) {
            if (result(i) != i * i) {
                return -1;
            }
        }

        assert(messages.size() == 10);
        for (size_t i = 0; i < messages.size(); i++) {
            long long square;
            float forty_two;
            unsigned long long one_forty_five;

            int scan_count = sscanf(messages[i].c_str(), "%lld the answer is %f unsigned %llu",
                                    &square, &forty_two, &one_forty_five);
            assert(scan_count == 3);
            assert(square == i * i);
            assert(forty_two == 42.0f);
            assert(one_forty_five == 145);
        }
    }

    messages.clear();

    {
        Func f;

        Param<void *> random_handle;
        random_handle.set((void *)127);

        // Test a string containing a printf format specifier (It should print it as-is).
        f(x) = print_when(x == 3, x * x, "g", 42.0f, "%s", random_handle);
        f.set_custom_print(halide_print);
        Image<int32_t> result = f.realize(10);

        for (int32_t i = 0; i < 10; i++) {
            if (result(i) != i * i) {
                return -1;
            }
        }

        assert(messages.size() == 1);
        long long nine;
        float forty_two;
        void *ptr;

        int scan_count = sscanf(messages[0].c_str(), "%lld g %f %%s %p",
                                &nine, &forty_two, &ptr);
        assert(scan_count == 3);
        assert(nine == 9);
        assert(forty_two == 42.0f);
        assert(ptr == (void *)127);

    }

    messages.clear();

    {
        Func f;

        // Test a single message longer than 8K.
        std::vector<Expr> args;
        for (int i = 0; i < 500; i++) {
            Expr e = cast<uint64_t>(i);
            e *= e;
            e *= e;
            e *= e;
            e *= e;
            args.push_back(e + 100);
            e = cast<double>(e);
            e *= e;
            e *= e;
            args.push_back(e);
        }
        f(x) = print(args);
        f.set_custom_print(halide_print);
        Image<uint64_t> result = f.realize(1);

        if (result(0) != 100) {
            return -1;
        }

        assert(messages.back().size() == 8191);
    }

    messages.clear();

    // Check that Halide's stringification of floats and doubles
    // matches %f and %e respectively.

    {
        Func f, g;

        const int N = 1000000;

        Expr e = reinterpret(Float(32), random_int());
        // Make sure we cover some special values.
        e = select(x == 0, 0.0f,
                   x == 1, -0.0f,
                   x == 2, 1.0f/0.0f,
                   x == 3, (-1.0f)/0.0f,
                   x == 4, (0.0f)/(0.0f),
                   x == 5, (-0.0f)/(0.0f),
                   x == 6, std::numeric_limits<float>::max(),
                   x == 7, std::numeric_limits<float>::min(),
                   x == 8, -std::numeric_limits<float>::max(),
                   x == 9, -std::numeric_limits<float>::min(),
                   e);

        f(x) = print(e);

        f.set_custom_print(halide_print);
        Image<float> imf = f.realize(N);

        assert(messages.size() == N);

        char correct[1024];
        for (int i = 0; i < N; i++) {
            snprintf(correct, sizeof(correct), "%f\n", imf(i));
            if (messages[i] != correct) {
                printf("float %d: %s vs %s for %10.20e\n", i, messages[i].c_str(), correct, imf(i));
                //return -1;
            }
        }

        messages.clear();

        g(x) = print(reinterpret(Float(64), (cast<uint64_t>(random_int()) << 32) | random_int()));
        g.set_custom_print(halide_print);
        Image<double> img = g.realize(N);

        assert(messages.size() == N);

        for (int i = 0; i < N; i++) {
            snprintf(correct, sizeof(correct), "%e\n", img(i));
            if (messages[i] != correct) {
                printf("double %d: %s vs %s for %10.20e\n", i, messages[i].c_str(), correct, img(i));
                //return -1;
            }
        }


    }


    printf("Success!\n");
    return 0;
}
