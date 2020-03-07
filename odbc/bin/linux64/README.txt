DolphinDB ODBC Plugin

Import data from databases that support ODBC interface into DolphinDB.

Prerequisites

1) Linux (Ubuntu 16.04)

# install unixODBC library
sudo apt-get install unixodbc unixodbc-dev

# MS SQL Server ODBC drivers
sudo apt-get install tdsodbc

# PostgreSQL ODBC ODBC drivers
sudo apt-get install odbc-postgresql

# MySQL ODBC drivers
sudo apt-get install libmyodbc

# SQLite ODBC drivers
sudo apt-get install libsqliteodbc

2) Windows

Download and install MS SQL SERVER, PostgreSQL, MySQL, or SQLite ODBC drivers from their corresponding websites.

3) Regarding the location of ODBC plugin and its configuration file

Please make sure:

libODBC.so (Linux) or libODBC.dll (Windows) and odbc.cfg are under the folder DOLPHINDB_DIR/server/plugins/odbc/. 

The DolphinDB server executable and its dynamic library libDolphinDB.so (Linux) or libDolphinDB.dll(Windows) are under the folder DOLPHINDB_DIR/server/. 
					


Getting started --- MySQL server as an example

1. Load plugin into DolphinDB

You can use DolphinDB function "loadPlugin" to load the ODBC plugin. This function takes a plugin configuration file as input. For example, the following DolphinDB script loads the plugin configured by odbc.cfg:
loadPlugin("/DOLPHINDB_DIR/server/plugins/odbc/odbc.cfg")


2. Built-in functions

odbc::connect(connStr)
Take an ODBC connection string as the parameter, open up a connection via ODBC with this string and return the connection handle to the user. For more information regarding the format of connection string, please refer to https://www.connectionstrings.com/.

odbc::close(connHandle)
Close an ODBC connection associated with the handle.

odbc::execute(connHandle)
Execute a SQL statement via connHandle. No results are returned.

odbc::query(connHandle or connStr, querySql, [t])
Query the database via connHandle or connStr and return the results as a DolphinDB table.

connStr: an ODBC connection string
connHandle: a connection handle object
querySql: a SQL query string
t: a table. If it is specified, query results will be appended to the table. Please note that the table schema must be compatible with the results returned from ODBC.

You can omit the prefix odbc:: by importing ODBC module namespace with statement "use odbc". However, if the function name conflicts with a function name from a different module, you need to add prefix odbc:: to the function name.

3. Examples

# load DolphinDB ODBC plugin with plugin configuration file odbc.cfg
loadPlugin("/DOLPHINDB_DIR/server/plugins/odbc/odbc.cfg")

# import ODBC module
use odbc

# define a DB connection string
# note that the driver name could be different depending on the version of ODBC is installed. e.g. Driver=MySQL ODBC 8.0 ANSI Driver
connStr="Driver=MySQL;Data Source = mysql-employees;server=127.0.0.1;uid=[username];pwd=[password]database=employees";

# establish a DB connection
conn=connect(connStr)

# query table "employees"
mysqlTable=query(conn,"select * from employees") 
select * from mysqlTable
close(conn)



