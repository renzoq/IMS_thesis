# IMS_thesis
This repository contains all information for running the IMS software, including three example domains expressed in LTLf and Python scripts for generating the Triangle-Tireworld and slippery world domain as LTLf expressions, which can be scaled dynamically.

This software can be used for managing and dynamically adding and dropping intentions to a list of intentions, for which intentions can be expressed as any LTLf formula, which includes temporally extended goals. 

This software is based on Syftmax. In order to use the code of the IMS, Syftmax should first be installed. Instructions can be found [here](https://github.com/Shufang-Zhu/SyftMax).
Important here is to note that during development and testing, the IMS software has only been tested and developed on Ubuntu. In order to avoid any complications, it is suggested to run this software on Ubuntu also.

## steps for running the IMS software
1. Install Syftmax as described earlier. Instructions are found [here](https://github.com/Shufang-Zhu/SyftMax).
2. Check if the software works correctly. Otherwise, revise if instructions have been followed correctly
3. Open the folder for Syftmax. Within this main folder for the project, place all contents of folder 'example' of our repository within the folder 'example' of Syftmax
4. In the general folder of Syftmax, open folder 'src'
5. In this folder, replace 'Main.cpp' of Syftmax with the 'Main.cpp'-file of this repository
6. Paste 'progression.cpp' and 'progression.h' in the same folder as 'Main.cpp'
7. The software should now be able to run as required

## using the Python scripts for generating domains in LTLf
The Python scripts for generating domains in LTLf can be found in this repository in the folder 'Python domain generation'. In this files, instructions are provided on how to alter and generate these LTLf specifications. 
