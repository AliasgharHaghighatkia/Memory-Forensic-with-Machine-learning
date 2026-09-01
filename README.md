# Memory Forensic with Machine Learning

A Windows-based memory dump analysis project developed in **C++ using the Win32 API**.

<img width="768" height="630" alt="Screenshot 2026-08-29 125436" src="[https://github.com/user-attachments/assets/d1a91a98-b70e-4f1b-b531-8e61b55a2a23](https://github.com/AliasgharHaghighatkia/Memory-Forensic-with-Machine-learning/blob/main/Images/Pslist.png)" />


## Overview

This project is being developed as a **Memory Forensics** tool for Windows.

The current version provides a Win32 GUI application that allows the user to select and receive a Windows memory dump (`.dmp`) file, display basic information about the selected dump, and perform basic memory analysis using **Volatility 3**.

The project is planned to be extended with advanced memory forensic analysis and machine learning capabilities.

## Current Features

* Win32 GUI application
* C++ / Windows API
* Select Windows `.dmp` files
* Open memory dump files in read-only mode
* Display:

  * File name
  * File path
  * File size
* Visual Studio project structure
* Git/GitHub version control
* **Volatility 3 integration**
* **Windows process enumeration using `windows.pslist`**
* **JSON-formatted Volatility output**

## Planned Features

* Memory dump header analysis
* Architecture detection (`x86` / `x64`)
* Process enumeration
* Thread analysis
* Loaded module analysis
* Suspicious memory detection
* Memory forensic indicators
* Automated analysis
* Extended Volatility 3 plugin integration
* Machine learning-based classification
* Forensic report generation
