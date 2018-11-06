CXX = clang-cl
CXXFLAGS += -utf-8 -std:c++latest -EHsc -GR- -W4 -Werror=gnu -Wmicrosoft -Wno-missing-field-initializers -Wpedantic -Wno-unused-function

MakeVHDX: MakeVHDX.cpp ConvertImage.cpp
clean:
	rm MakeVHDX.exe