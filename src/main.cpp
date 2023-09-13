#include <pybind11/pybind11.h>
namespace py = pybind11;

int add(int a, int b) { return a + b; }

PYBIND11_MODULE(_core, m)
{
    m.doc() = "My Test C++ Extension";
    m.def("add", &add, "Add two numbers together");
}
