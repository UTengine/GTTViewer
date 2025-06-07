# GTTViewer
Initial release
GTTViewer and parser Open Source 
Credits Wincrypt class: Twostars
Rest of the project: Tahsin

Issues:
Too high miplevel/lodlevel will make the rendered textures output invalid picture data,
either I forgot to decrypt some certain miplevel or its caused by dynamically created mips,
it is not an issue for the use case and I forced it to use the highest miplevel combined with antialiasing 8x

Features:
Zoom works
Panning works
LogFile works
Debug messages
Messageboxes
Show offsets of every beginning texture you can use HXD hex editors to replace them by going to the offset addresses.
DX11
Shaders
Multi textures GTT NTF3-NTF7
Single textures DXT NTF3-NTF7