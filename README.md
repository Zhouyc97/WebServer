# WebServer   
#添加数据库    
create database yourdb;   
USE yourdb;   
CREATE TABLE user(    
    username char(50) NULL,   
    passwd char(50) NULL    
);   
INSERT INTO user(username, passwd) VALUES('name', 'passwd');

//更改数据库用户名密码,webserver.cpp line23 ~ line26  
m_user = "root";    
m_passWord = "1";   
m_databaseName = "yourdb";   

#运行   
make    
./server    
ip:9006    

