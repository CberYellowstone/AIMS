#include "database.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

namespace Database {
    database::database(const QString &path) {
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(path);

        if (!db.open()) {
            qDebug() << "database.cpp: Error: connection with database fail";
        } else {
            qDebug() << "database.cpp: 数据库连接成功";
            initializeDatabase();
        }
    }

    Status database::getStudentById(const QString &id, Student &student) {
        QSqlQuery query;
        query.prepare("SELECT * FROM student_information WHERE StudentId = :id");
        query.bindValue(":id", id);
        if (query.exec() && query.next()) {
            QSqlRecord record = query.record();
            student.Id = record.value("StudentId").toString();
            student.Name = record.value("StudentName").toString();
            //根据国标GB/T 2261.1-2003，记录中的 0、1、2、9 对应 未知、男、女、其他
            student.Sex = record.value("StudentSex").toInt() == 0 ? "未知" :
                          record.value("StudentSex").toInt() == 1 ? "男" :
                          record.value("StudentSex").toInt() == 2 ? "女" : "其他";
            student.College = record.value("StudentCollege").toString();
            student.Major = record.value("StudentMajor").toString();
            student.Class = record.value("StudentClass").toString();
            student.Age = record.value("StudentAge").toInt();
            student.PhoneNumber = record.value("StudentPhoneNumber").toString();
            student.DormitoryArea = record.value("DormitoryArea").toString();
            student.DormitoryNum = record.value("DormitoryNum").toString();

            QString chosenLessonsJson = record.value("ChosenLessons").toString();
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(chosenLessonsJson.toUtf8(), &jsonError);
            QJsonArray array = doc.array();

            student.ChosenLessons.clear();
            for (auto &&i: array) {
                student.ChosenLessons.append(i.toString());
            }
            return Success;
        }
        return ERROR;
    }

    Status database::initializeDatabase() {
        QSqlQuery query;
        QStringList tableCreationQueries = {
                R"(
            CREATE TABLE IF NOT EXISTS student_information (
                StudentId TEXT NOT NULL UNIQUE,
                StudentName TEXT NOT NULL,
                StudentSex INTEGER NOT NULL CHECK(StudentSex in (0, 1, 2, 9)),
                StudentCollege TEXT NOT NULL,
                StudentMajor TEXT NOT NULL,
                StudentClass TEXT NOT NULL,
                StudentAge INTEGER CHECK(StudentAge > 0),
                "StudentPhoneNumber" TEXT UNIQUE,
                DormitoryArea TEXT NOT NULL,
                DormitoryNum TEXT,
                ChosenLessons TEXT NOT NULL,
                PRIMARY KEY(StudentId)
            )
        )",
                R"(
            CREATE TABLE IF NOT EXISTS lesson_information (
                LessonId TEXT NOT NULL UNIQUE,
                LessonName TEXT NOT NULL,
                TeacherId TEXT NOT NULL,
                LessonCredits INTEGER NOT NULL,
                LessonArea TEXT,
                LessonTimeAndLocations TEXT,
                LessonStudents TEXT,
                PRIMARY KEY(LessonId),
                FOREIGN KEY (TeacherId) REFERENCES teacher_information(TeacherId)
                ON UPDATE NO ACTION ON DELETE NO ACTION
            )
        )",
                R"(
            CREATE TABLE IF NOT EXISTS teacher_information (
                TeacherId TEXT NOT NULL UNIQUE,
                TeacherName TEXT NOT NULL,
                TeachingLessons TEXT NOT NULL,
                PRIMARY KEY(TeacherId)
            )
        )",
                R"(
            CREATE TABLE IF NOT EXISTS auth (
                Id INTEGER NOT NULL UNIQUE,
                Account TEXT NOT NULL,
                Secret TEXT NOT NULL,
                AccountType TEXT CHECK(AccountType in ('Teacher', 'Student')) NOT NULL,
                IsSuper INTEGER NOT NULL,
                PRIMARY KEY(Id)
            )
        )"};

        QStringList tableNames = {"student_information", "lesson_information",
                                  "teacher_information", "auth"};

        for (int i = 0; i < tableCreationQueries.size(); i++) {
            if (db.tables().contains(tableNames[i])) {
                qDebug() << "database.cpp: 表" << tableNames[i] << "已存在";
                continue;
            }

            qDebug() << "database.cpp: 正在创建" << tableNames[i];
            if (!query.exec(tableCreationQueries[i])) {
                qDebug() << "database.cpp: Error: " << query.lastError();
                return ERROR;
            } else {
                qDebug() << tableNames[i] << "创建成功";
            }
        }
        return Success;
    }

    Status database::updateStudent(const Student &student) {
        QSqlQuery query;
        query.prepare(
                "INSERT OR REPLACE INTO student_information (StudentId, StudentName, StudentSex, StudentCollege, StudentMajor, StudentClass, StudentAge, StudentPhoneNumber, DormitoryArea, DormitoryNum, ChosenLessons) "
                "VALUES (:id, :name, :sex, :college, :major, :class, :age, :phoneNumber, :dormitoryArea, :dormitoryNum, :chosenLessons)");

        query.bindValue(":name", student.Name);
        query.bindValue(":sex", student.Sex == "男" ? 1 : student.Sex == "女" ? 2 : student.Sex == "其他" ? 9 : 0);
        query.bindValue(":college", student.College);
        query.bindValue(":major", student.Major);
        query.bindValue(":class", student.Class);
        query.bindValue(":age", student.Age);
        query.bindValue(":phoneNumber", student.PhoneNumber);
        query.bindValue(":dormitoryArea", student.DormitoryArea);
        query.bindValue(":dormitoryNum", student.DormitoryNum);

        QJsonArray chosenLessonsArray;
        for (const auto &lesson: student.ChosenLessons) {
            chosenLessonsArray.append(lesson);
        }
        QJsonDocument chosenLessonsDoc(chosenLessonsArray);
        QString chosenLessonsJson(chosenLessonsDoc.toJson(QJsonDocument::Compact));
        query.bindValue(":chosenLessons", chosenLessonsJson);

        query.bindValue(":id", student.Id);

        if (!query.exec()) {
            qDebug() << "database.cpp: updateStudent error: " << query.lastError();
            return ERROR;
        }

        return Success;
    }
}// namespace Database