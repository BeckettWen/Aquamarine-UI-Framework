# Aquamarine-UI-Experimental
**This UI Framework is still in Experimental Phase and still in Construction**

***

**Introduction:**  
This project is built to offer a light weight GUI experience mainly in temporary testing.  
The GUI Framework is completely build on the OpenGL Framework and on macOS platform.


**Usage:**  
In the AquamarineUI/AquamarineUI, there will be the header file and the C++ source file  
To run the code correctly, you need the dependency of OpenGL, GLEW, GLFW. So make sure you have the environment configured  
  
Copy the header file and the C++ file to your directory and use the function inside it  
  
the "Main.cpp" contains the example of the Aquamarine UI, **DO NOT** copy that if you don't need it
  
```cpp
//Creating the basic window
//Parameter Explanation(window width, window height, color array, transparency, window title)
Aquamarine_Window exampleWindow = Aquamarine_Window();
exampleWindow.CreateCustomizedWindow(1366, 768, RGB, 1.0f, "Default Window Title");

```
