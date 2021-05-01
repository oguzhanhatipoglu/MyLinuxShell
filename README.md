# MyLinuxShell

We take PATH and tokenize it then make tmp array which will keep the path of the child execute process. We use access like mentioned in Ps.
We implement a command named hello_Owner, it opens the sites on web browser which we declare to open it. We are in pandemic time and taking online education and because of online education we are using our pcs too much and whenever we open it we need to go browser and open specific sites like blackboard, kusis etc and we believe it’s a very time consuming thing in online education. Our function taking args as ‘s’,’w’ meaned work day and school day it open specific pages for our usage, we can easily make this implementation to user interface, like in shortdir function, we can set the sites which we want to open when reboot the computer or we can delete sites from specified days like work or school and also we can implement new args like ‘sport’. Its easy to implement to add this functuality to our code because, codes of set,delete are ready in shortdir function but because of time lack (midterms, and homeworks) we cannot implement this, but like we said its easy to implement this, codes are ready. We use path to open firefox from different args and we used system call for open firefox, we faced with errors like opening GUI in code from terminal but we solved it.

**shortdir: In this part, you will implement a new command, shortdir, to asso-
ciate short names with the current directory. The purpose is to reach the directory (change
1
 - seashell - Your Custom Shell ( Spring 2021): Project 1 DESCRIPTION
 to the directory) with a short name instead of typing the whole path. For instance, we associate the name matlab with the directory /usr/local/MATLAB/R2018b/ and use it to change to that directory by using the name matlab. You will also implement supportive options for the shortdir as follows:
• shortdir set name: associates name with the current directory. Overwrites an existing association.
• shortdir jump name: changes to the directory associated with name.
• shortdir del name: deletes the name-directory association.
• shortdir clear: deletes all the name-directory associations.
• shortdir list: lists all the name-directory associations.
Note that this command lives across shell sessions; when a new shell session is started, it should remember the associations from the previous sessions. Refer to Listing 1 for sample usage of shortdir.**
