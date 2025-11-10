#!/bin/bash
#!/bin/bash
read -p "Enter the text port number: " textport
read -p "Enter the file port number: " fileport

sudo ufw allow "$textport"/tcp
sudo ufw allow "$fileport"/tcp

echo "Ports $textport and $fileport are now allowed through UFW."

