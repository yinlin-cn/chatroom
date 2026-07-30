create database chat;
use chat;
create table login(
id INT PRIMARY KEY AUTO_INCREMENT,
username varchar(50) not null unique,
mykey varchar(50)  not null);
create index username_index on login(username);
create table chat_message(
id INT PRIMARY KEY AUTO_INCREMENT,
username varchar(50) not null,
message varchar(500) not null,
ctime  datetime default now())ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
create index ctime_index on chat_message(ctime);
create table person_message(
id INT PRIMARY KEY AUTO_INCREMENT,
username  varchar(50) not null unique,
birth datetime default NULL,
notes varchar(255))ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
create index username_index_p on person_message(username);
SELECT username, message, ctime
FROM chat_message
WHERE ctime < '2000-01-01 00:00:00'  
ORDER BY ctime DESC              
LIMIT 10;       
insert into login(username,mykey)
value("yinglin","123456");     
delete from login where id=1;
select * from login;
