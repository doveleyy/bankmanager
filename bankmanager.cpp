#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <sqlite3.h>

// Prototype Class
class User;

struct CallbackData {
    User* userPtr;
    int* countPtr;
    std::unordered_map<int, int>* idMapPtr;
};

//Globals
static sqlite3* db = nullptr;
char* errMsg = nullptr;
bool callbackCalled = false;

enum class Commands {
    createuser,
    login,
    logout,
    createaccount,
    selectaccount,
    balance,
    deposit,
    withdraw,
    help,
    quit,
    Count
};

//Prototypes
Commands inputToCommand(const std::string& str);
Commands getCommand();
void call(void);
void checkdb(int rc, const char* errMsg);
void initialize(void);
void createUser(void);
void login(void);
int findUser_callback(void* data, int argc, char** argv, char** column);
void help(void);

const std::unordered_map<std::string, Commands> commandMap {
    {"-createuser", Commands::createuser},
    {"-login", Commands::login},
    {"-logout", Commands::logout},
    {"-createaccount", Commands::createaccount},
    {"-selectaccount", Commands::selectaccount},
    {"-balance", Commands::balance},
    {"-deposit", Commands::deposit},
    {"-withdraw", Commands::withdraw},
    {"-help", Commands::help},
    {"-quit", Commands::quit}
};

class User {
private:
    int login_state; //Logged out = 0, Logged in = 1
    std::string username;
    std::vector<std::string> useraccounts;
    std::string currentaccount;
    int userid;
    int accountid;

public:
    User(): login_state{0}, username{""}, useraccounts({}), currentaccount{""}, userid{0}, accountid{0} {}

    void login(std::string uname) {
        if (login_state == 0) {
            this->username = uname;
            this->login_state = 1;
            User::showuser(this->username);
        } else {
            std::cout << "Already logged in as " << this->username << "\n";
        }
    }

    void logout(void) {
        if (login_state == 1) {
            std::cout << "Logged out of " << this->username << "\n";
            this->username = "";
            this->login_state = 0;
            this->userid = 0;
            this->accountid = 0;
        } else {
            std::cerr << "Error: Not logged in" << std::endl;
        }
    }
    
    static void showuser(const std::string& username) {
        std::cout << "Logged in to " << username << "\n";
    }

    void setuserid(int id) {
        this->userid = id;
    }

    int createaccount(void) {
        if (login_state == 1) {
            std::string createQuery =
            "INSERT INTO accounts (balance, userid) "
            "VALUES ('0', '" + std::to_string(this->userid) + "')";

            int rc = sqlite3_exec(db, createQuery.c_str(), nullptr, nullptr, &errMsg);
            checkdb(rc, errMsg);

            const char* rowCountQuery = "SELECT last_insert_rowid()";
            int rowCount;

            rc = sqlite3_exec(db, rowCountQuery, rowCheck_callback, &rowCount, &errMsg);
            checkdb(rc, errMsg);

            std::cout << "Account successfully created\n";
            std::cout << "Account ID: " << rowCount << "\n";
            return rowCount;
        } else {
            std::cout << "Login to user to create bank account\n";
            return -1;
        }
    }

    static int rowCheck_callback(void* data, int argc, char** argv, char** column) {
        int* rowCountPtr = static_cast<int*>(data);
        *rowCountPtr = std::stoi(argv[0]);
        return 0;
    }


    void selectaccount(void) {
        std::string createQuery =
        "SELECT * FROM accounts WHERE userid = '" + std::to_string(userid) + "'";
        
        int count = 1;
        std::unordered_map<int, int> idMap; 

        CallbackData callbackData = {this, &count, &idMap};

        int rc = sqlite3_exec(db, createQuery.c_str(), selectAccount_callback, &callbackData, &errMsg);
        checkdb(rc, errMsg);

        std::string reply;

        if (!callbackCalled) {
            do {
                std::cout << "No accounts for this user, create one? (y/n)\n";
                std::cin >> reply;
            } while (reply != "y" && reply != "n");
        }

        if (reply == "y") {
            int createdid = createaccount();
            this->accountid = createdid;
            std::cout << "Account with ID " << this->accountid << " selected\n";
            return;
        } else if (reply == "n") {
            return;
        }

        callbackCalled = false;

        int accountinput;
        
        do {
            std::cout << "Enter the account number according to its index: ";
            std::cin >> accountinput;
            auto it = idMap.find(accountinput);
            if (it != idMap.end()) {
                std::cout << "Account with id: " << it->second << " selected\n";
                this->accountid = it->second;
                break;
            }
        } while(true);
    }

    static int selectAccount_callback(void* data, int argc, char** argv, char** column) {
        CallbackData* callbackData = static_cast<CallbackData*>(data);
        User* user = callbackData->userPtr;
        int* count = callbackData->countPtr;
        std::unordered_map<int, int>* idMapPtr = callbackData->idMapPtr;

        callbackCalled = true;
        std::cout << "Account " << *count << " id: " << argv[0] << "\n";
        idMapPtr->insert({*count, std::stoi(argv[0])});
        (*count)++;
        return 0;
    }

    void returnbalance(void) {
        std::string createQuery =
        "SELECT * FROM accounts WHERE userid = '" + std::to_string(userid) + "' "
        "AND id = '" + std::to_string(accountid) + "'";

        int rc = sqlite3_exec(db, createQuery.c_str(), returnBalance_callback, nullptr, &errMsg);
        checkdb(rc, errMsg);
    }
    
    static int returnBalance_callback(void* data, int argc, char** argv, char** column) {
        std::cout << "Your balance is: " << argv[1] << "\n";
        return 0;
    }
};

class quitException : public std::exception {
private:
    std::string message;
public:
    quitException(std::string msg) : message(msg) {}

    const char* what() const noexcept override {
        return message.c_str();
    }
};

Commands inputToCommand(const std::string& str) {
    auto it = commandMap.find(str);
    if (it != commandMap.end()) {
        return it->second;
    }
    throw std::invalid_argument("Invalid Command");
}

Commands getCommand() {
    while (true) {
        std::string commandInput;
        std::cout << "Please type a command (command list in -help)\n";
        std::cin >> commandInput;

        try {
            Commands command = inputToCommand(commandInput);
            return command;
        } catch (const std::invalid_argument& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

//Globals
User CurrentUser;

//Call functions
void call(void) {
    Commands command = getCommand();
    switch (command) {
        case Commands::createuser:
            createUser();
            break;
        case Commands::login:
            login();
            break;
        case Commands::logout:
            CurrentUser.logout();
            break;
        case Commands::createaccount:
            CurrentUser.createaccount();
            break;
        case Commands::selectaccount:
            CurrentUser.selectaccount();
            break;
        case Commands::balance:
            CurrentUser.returnbalance();
            break;
        case Commands::deposit:
            std::cout << "deposit\n";
            break;
        case Commands::withdraw:
            std::cout << "withdraw\n";
            break;
        case Commands::help:
            help();
            break;
        case Commands::quit:
            throw quitException("Quitting the program");
    }
}

void checkdb(int rc, const char* errMsg) {
    if (rc == SQLITE_CONSTRAINT) {
        std::cerr << "The username has been taken " << errMsg << std::endl;
        sqlite3_free(const_cast<char*>(errMsg));
    } else if (rc != SQLITE_OK) {
        std::cout << "return code is " << rc << "\n";
        std::cerr << "Database Error (restart program): " << errMsg << std::endl;
        sqlite3_free(const_cast<char*>(errMsg));
        sqlite3_close(db);
    }
}

void initialize(void) {
    int rc = sqlite3_open("bankdetails.db", &db);
    if (rc != SQLITE_OK) { // SQLITE_OK evals to 0, rc != 0 means failed
        std::cerr << "Unable to access database, restart program";
        sqlite3_close(db);
    }
    
    const char* createUserTable = 
        "CREATE TABLE IF NOT EXISTS users "
        "(id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT NOT NULL, "
        "password TEXT NOT NULL, "
        "CONSTRAINT unique_username UNIQUE (username))";

    const char* createAccountTable =
        "CREATE TABLE IF NOT EXISTS accounts"
        "(id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "balance INTEGER NOT NULL, "
        "userid INTEGER NOT NULL, "
        "FOREIGN KEY(userid) REFERENCES users(id))";

    const char* createTransactionsTable =
        "CREATE TABLE IF NOT EXISTS transactions"
        "(id INTEGER PRIMARY KEY, "
        "transactiontype INTEGER NOT NULL, "
        "accounttransferring INTEGER NOT NULL, "
        "accountreceiving INTEGER NOT NULL, "
        "amount INTEGER NOT NULL, "
        "FOREIGN KEY(accounttransferring) REFERENCES accounts(id), "
        "FOREIGN KEY(accountreceiving) REFERENCES accounts(id))";

    rc = sqlite3_exec(db, createUserTable, nullptr, nullptr, &errMsg);
    checkdb(rc, errMsg);

    rc = sqlite3_exec(db, createAccountTable, nullptr, nullptr, &errMsg);
    checkdb(rc, errMsg);

    rc = sqlite3_exec(db, createTransactionsTable, nullptr, nullptr, &errMsg);
    checkdb(rc, errMsg);

}

//CREATE USER
void createUser(void) {
    std::string username, password;
    std::cout << "Enter Username:";
    std::cin >> username;
    std::cout << "Enter Password:";
    std::cin >> password;

    std::string createQuery = 
    "INSERT INTO users (username, password) VALUES (?, ?)";

    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db, createQuery.c_str(), -1, &stmt, nullptr);
    checkdb(rc);

    rc = sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Error binding password: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return;
    }

    rc = sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Error binding password: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return;
    }
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Error executing statement: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
}

//LOGIN
void login(void) {
    std::string username, password;
    std::cout << "Enter Username:";
    std::cin >> username;
    std::cout << "Enter Password:";
    std::cin >> password;

    std::string createQuery =
    "SELECT * FROM users WHERE username = '" + username + "' AND password = '" + password + "'";

    int rc = sqlite3_exec(db, createQuery.c_str(), findUser_callback, nullptr, &errMsg);
    checkdb(rc, errMsg);

    //Flag for callback function to send error message
    if (!callbackCalled) {
        std::cout << "Invalid user or password" << "\n";
    } else {
        callbackCalled = false;
        CurrentUser.selectaccount();
    }

    callbackCalled = false;
}

int findUser_callback(void* data, int argc, char** argv, char** column) {
    if (argc > 0 && argv != nullptr) {
        callbackCalled = true;
        CurrentUser.login(std::string(argv[1]));
        CurrentUser.setuserid(std::stoi(argv[0]));
    }
    return 0;
}

void help(void) {
    std::cout << "List of commands: \n";
    for (const auto& pair : commandMap) {
        std::cout << pair.first << "\n";
    }
}


int main(void) {
    initialize();
    while(true) {
        try {
            call();
        } catch (const quitException& e) {
            std::cout << e.what() << "\n";
            return 0;
        }
    }
}