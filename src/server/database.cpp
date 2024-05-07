#include "database.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QJsonObject>

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

    bool database::ifTableExist(const QString &tableName) {
        QSqlQuery query;
        query.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name=:name");
        query.bindValue(":name", tableName);
        if (query.exec() && query.next()) {
            qDebug() << "database.cpp: 表" << tableName << "已存在";
            return true;
        }
        return false;
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
                ChosenLessons TEXT NOT NULL DEFAULT '[]',
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
                LessonTimeAndLocations TEXT NOT NULL DEFAULT '{}',
                LessonStudents TEXT NOT NULL DEFAULT '[]',
                PRIMARY KEY(LessonId),
                FOREIGN KEY (TeacherId) REFERENCES teacher_information(TeacherId)
                ON UPDATE NO ACTION ON DELETE NO ACTION
            )
        )",
                R"(
            CREATE TABLE IF NOT EXISTS teacher_information (
                TeacherId TEXT NOT NULL UNIQUE,
                TeacherName TEXT NOT NULL,
                TeachingLessons TEXT NOT NULL DEFAULT '[]',
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
            if (!ifTableExist(tableNames[i])) {
                qDebug() << "database.cpp: 正在创建" << tableNames[i];

                if (!query.exec(tableCreationQueries[i])) {
                    qDebug() << "database.cpp: Error: " << query.lastError();
                    return ERROR;
                } else {
                    qDebug() << tableNames[i] << "创建成功";
                }
            }
        }
        return Success;
    }

    Status database::updateStudent(const Student &student) {
        QSqlQuery query;
        query.prepare(
                "INSERT OR REPLACE INTO student_information (StudentId, StudentName, StudentSex, StudentCollege, StudentMajor, StudentClass, StudentAge, StudentPhoneNumber, DormitoryArea, DormitoryNum) "
                "VALUES (:id, :name, :sex, :college, :major, :class, :age, :phoneNumber, :dormitoryArea, :dormitoryNum)");

        query.bindValue(":name", student.Name);
        query.bindValue(":sex", student.Sex == "男" ? 1 : student.Sex == "女" ? 2 : student.Sex == "其他" ? 9 : 0);
        query.bindValue(":college", student.College);
        query.bindValue(":major", student.Major);
        query.bindValue(":class", student.Class);
        query.bindValue(":age", student.Age);
        query.bindValue(":phoneNumber", student.PhoneNumber);
        query.bindValue(":dormitoryArea", student.DormitoryArea);
        query.bindValue(":dormitoryNum", student.DormitoryNum);

        query.bindValue(":id", student.Id);

        if (!query.exec()) {
            qDebug() << "database.cpp: updateStudent error: " << query.lastError();
            return ERROR;
        }

        return Success;
    }

    Status database::updateLessonInformation(const Lesson &lesson) {
        QSqlQuery query;
        query.prepare(
                "INSERT OR REPLACE INTO lesson_information (LessonId, LessonName, TeacherId, LessonCredits, LessonArea, LessonTimeAndLocations) "
                "VALUES (:id, :name, :teacherId, :credits, :area, :timeAndLocations)");
        query.bindValue(":id", lesson.Id);
        query.bindValue(":name", lesson.LessonName);
        query.bindValue(":teacherId", lesson.TeacherId);
        query.bindValue(":credits", lesson.LessonCredits);
        query.bindValue(":area", lesson.LessonArea);

        //lesson.LessonTimeAndLocations 是 QMap<QString, QVector<QString>> 类型
        QJsonObject timeAndLocationsObj;
        // 遍历QMap
        for (auto it = lesson.LessonTimeAndLocations.cbegin(); it != lesson.LessonTimeAndLocations.cend(); ++it) {
            // 将QVector<QString>转换为QJsonArray
            QJsonArray jsonArray;
            for (const auto &str: it.value()) {
                jsonArray.append(QJsonValue(str));
            }
            // 将QJsonArray添加到QJsonObject中
            timeAndLocationsObj.insert(it.key(), jsonArray);
        }
        // 将QJsonObject转换为JSON字符串
        QJsonDocument doc(timeAndLocationsObj);
        QString timeAndLocationsJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

        // 绑定到查询
        query.bindValue(":timeAndLocations", timeAndLocationsJson);

        if (!query.exec()) {
            qDebug() << "database.cpp: updateLessonInformation error: " << query.lastError();
            return ERROR;
        }
        QString tableName = "lesson_" + lesson.Id;
        Status status = createTableIfNotExists(tableName);
        if (status != Success) {
            return status;
        }
        //更新老师的教课信息
        status = addTeachingLesson(lesson.TeacherId, lesson.Id);
        if (status != Success) {
            return status;
        }
        return Success;
    }

    Status database::createTableIfNotExists(const QString &tableName) {
        QSqlQuery query;
        if (!ifTableExist(tableName)) {
            // 如果不存在，创建新表
            qDebug() << "database.cpp: 正在创建表" << tableName;
            query.prepare(R"(
        CREATE TABLE IF NOT EXISTS ")" + tableName + R"(" (
            "StudentId" TEXT NOT NULL UNIQUE,
            "ExamGrade" REAL,
            "RegularGrade" REAL,
            "TotalGrade" REAL,
            "Retake" INTEGER,
            "RetakeSemesters" TEXT NOT NULL DEFAULT '[]',
            "RetakeGrades" TEXT NOT NULL DEFAULT '[]',
            PRIMARY KEY("StudentId"),
            FOREIGN KEY ("StudentId") REFERENCES "student_information"("StudentId")
            ON UPDATE NO ACTION ON DELETE NO ACTION
        )
    )");
            if (!query.exec()) {
                qDebug() << "database.cpp: Error creating table " << tableName << ": " << query.lastError();
                return ERROR;
            }
            qDebug() << "database.cpp: 表" << tableName << "创建成功";
        }
        return Success;
    }

    Status database::getLessonById(const QString &id, Lesson &lesson) {
        QSqlQuery query;
        query.prepare("SELECT * FROM lesson_information WHERE LessonId = :id");
        query.bindValue(":id", id);
        if (query.exec() && query.next()) {
            QSqlRecord record = query.record();
            lesson.Id = record.value("LessonId").toString();
            lesson.LessonName = record.value("LessonName").toString();
            lesson.TeacherId = record.value("TeacherId").toString();
            lesson.LessonCredits = record.value("LessonCredits").toInt();
            lesson.LessonArea = record.value("LessonArea").toString();

            QString lessonTimeAndLocationsJson = record.value("LessonTimeAndLocations").toString();
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(lessonTimeAndLocationsJson.toUtf8(), &jsonError);
            //lessonTimeAndLocationsJson格式如下：{"1-6周":["40809节","4501"],"7-10周":["30609节","4601"]}

            QJsonObject obj = doc.object();
            QMap<QString, QVector<QString>> timeAndLocationsMap;

            for (auto it = obj.begin(); it != obj.end(); ++it) {
                QJsonArray timeAndLocationArray = it.value().toArray();
                QVector<QString> timeAndLocation;
                for (auto &&i: timeAndLocationArray) {
                    timeAndLocation.append(i.toString());
                }
                timeAndLocationsMap.insert(it.key(), timeAndLocation);
            }

            lesson.LessonTimeAndLocations = timeAndLocationsMap;

            QString lessonStudentsJson = record.value("LessonStudents").toString();
            doc = QJsonDocument::fromJson(lessonStudentsJson.toUtf8(), &jsonError);
            QJsonArray array = doc.array();
            lesson.LessonStudents.clear();
            for (auto &&i: array) {
                lesson.LessonStudents.append(i.toString());
            }
            return Success;
        }
        return ERROR;
    }

    Status database::updateTeachingLessons(const QString &teacherId, const QVector<QString> &teachingLessons) {
        QSqlQuery query;
        query.prepare("UPDATE teacher_information SET TeachingLessons = :teachingLessons WHERE TeacherId = :teacherId");
        QJsonArray teachingLessonsArray;
        for (const auto &lesson: teachingLessons) {
            teachingLessonsArray.append(lesson);
        }
        QJsonDocument teachingLessonsDoc(teachingLessonsArray);
        QString teachingLessonsJson(teachingLessonsDoc.toJson(QJsonDocument::Compact));
        query.bindValue(":teachingLessons", teachingLessonsJson);
        query.bindValue(":teacherId", teacherId);
        if (!query.exec()) {
            qDebug() << "database.cpp: updateTeachingLessons error: " << query.lastError();
            return ERROR;
        }
        return Success;
    }

    Status database::addTeachingLesson(const QString &teacherId, const QString &lessonId) {
        QSqlQuery query;
        query.prepare("SELECT TeachingLessons FROM teacher_information WHERE TeacherId = :teacherId");
        query.bindValue(":teacherId", teacherId);
        if (!query.exec() || !query.next()) {
            qDebug() << "database.cpp: addTeachingLesson error: " << query.lastError();
            return ERROR;
        }
        QString teachingLessonsJson = query.value("TeachingLessons").toString();
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(teachingLessonsJson.toUtf8(), &jsonError);
        QJsonArray array = doc.array();

        // Check if the lesson already exists
        for (const auto &existingLessonId: array) {
            if (existingLessonId.toString() == lessonId) {
                // If the lesson already exists, return Success
                return Success;
            }
        }

        // If the lesson does not exist, add it
        array.append(lessonId);
        QJsonDocument newDoc(array);
        QString newTeachingLessonsJson(newDoc.toJson(QJsonDocument::Compact));
        query.prepare("UPDATE teacher_information SET TeachingLessons = :teachingLessons WHERE TeacherId = :teacherId");
        query.bindValue(":teachingLessons", newTeachingLessonsJson);
        query.bindValue(":teacherId", teacherId);
        if (!query.exec()) {
            qDebug() << "database.cpp: addTeachingLesson error: " << query.lastError();
            return ERROR;
        }
        return Success;
    }

    Status database::deleteTeachingLesson(const QString &teacherId, const QString &lessonId) {
        QSqlQuery query;
        query.prepare("SELECT TeachingLessons FROM teacher_information WHERE TeacherId = :teacherId");
        query.bindValue(":teacherId", teacherId);
        if (!query.exec() || !query.next()) {
            qDebug() << "database.cpp: deleteTeachingLesson error: " << query.lastError();
            return ERROR;
        }
        QString teachingLessonsJson = query.value("TeachingLessons").toString();
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(teachingLessonsJson.toUtf8(), &jsonError);
        QJsonArray array = doc.array();
        for (int i = 0; i < array.size(); i++) {
            if (array[i].toString() == lessonId) {
                array.removeAt(i);
                break;
            }
        }
        QJsonDocument newDoc(array);
        QString newTeachingLessonsJson(newDoc.toJson(QJsonDocument::Compact));
        query.prepare("UPDATE teacher_information SET TeachingLessons = :teachingLessons WHERE TeacherId = :teacherId");
        query.bindValue(":teachingLessons", newTeachingLessonsJson);
        query.bindValue(":teacherId", teacherId);
        if (!query.exec()) {
            qDebug() << "database.cpp: deleteTeachingLesson error: " << query.lastError();
            return ERROR;
        }
        return Success;
    }

}// namespace Database