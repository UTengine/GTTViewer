# GTTViewer
Initial release
GTTViewer and parser Open Source 
Credits Wincrypt class: Twostars
Rest of the project: Tahsin

Issues:
Too high miplevel/lodlevel will make the rendered textures output invalid picture data,
either I forgot to decrypt some certain miplevel or its caused by dynamically created mips,
it is not an issue for the use case and I forced it to use the highest miplevel combined with antialiasing 8x
Blending issues after adding support for format 21 hero online.....
Might release v0.3 one of these days..

Features:
Zoom works
Panning works
LogFile works
Debug messages
Messageboxes
Show offsets of every beginning texture you can use HXD hex editors to replace them by going to the offset addresses.
DX11
Shaders
Multi textures GTT NTF3-NTF7 (Format 21 & 22 ??? not tested)
Single textures DXT NTF3-NTF7-Format 21 & 22
Hero Online compatability:

![image](https://github.com/user-attachments/assets/3218335b-e706-42b9-b4c7-42a210ab6a32)
![image](https://github.com/user-attachments/assets/d1d66b76-94c3-4b0c-8b31-caf250f2f603)
![image](https://github.com/user-attachments/assets/4843a6ba-602f-45ec-83f1-c66300f9c553)
![image](https://github.com/user-attachments/assets/a2474093-56fb-4220-8be4-79f5b824d0b3)
![image](https://github.com/user-attachments/assets/6735ee75-415a-4f71-aaa9-5db528f42ae1)


Knight Online compatability:

![image](https://github.com/user-attachments/assets/aa284a70-25a9-4613-aeed-7d232ecd80c8)
![image](https://github.com/user-attachments/assets/a123d2e7-e7a1-4e89-bfd3-9ade5da25b73)
![image](https://github.com/user-attachments/assets/13130342-fd88-4662-9f0e-c8c446ccc22c)
