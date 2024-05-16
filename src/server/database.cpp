#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-convert-member-functions-to-static"

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
            qDebug() << "Debug | database.cpp: Error: connection with database fail";
        } else {
            qDebug() << "Debug | database.cpp: 数据库连接成功";
            initializeDatabase();
        }
    }

    bool database::ifTableExist(const QString &tableName) {
        return db.tables().contains(tableName);
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
                StudentPhoneNumber TEXT NOT NULL,
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
                LessonSemester TEXT NOT NULL,
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
                TeacherUint TEXT,
                TeachingLessons TEXT NOT NULL DEFAULT '[]',
                PRIMARY KEY(TeacherId)
            )
        )",
                R"(
            CREATE TABLE IF NOT EXISTS auth (
                Account TEXT NOT NULL UNIQUE,
                Secret TEXT NOT NULL,
                AccountType INTEGER CHECK(AccountType in (0, 1)) NOT NULL,
                IsSuper INTEGER NOT NULL,
                PRIMARY KEY(Account)
            )
        )"};

        QStringList tableNames = {"student_information", "lesson_information",
                                  "teacher_information", "auth"};

        for (int i = 0; i < tableCreationQueries.size(); i++) {
            if (!ifTableExist(tableNames[i])) {
                qDebug() << "Debug | database.cpp: 正在创建" << tableNames[i];

                if (!query.exec(tableCreationQueries[i])) {
                    qDebug() << "Debug | database.cpp: Error:" << query.lastError();
                    return ERROR;
                } else {
                    qDebug() << "Debug | database.cpp:" << tableNames[i] << "创建成功";
                }
            }
        }
        return Success;
    }

    Status database::checkDatabase() {
        // 开始事务
        db.transaction();

        // 获取所有课程
        QSqlQuery query;
        query.prepare("SELECT LessonId, TeacherId FROM lesson_information");
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: checkDatabase error:" << query.lastError();
            db.rollback();
            return ERROR;
        }
        QMap<QString, QVector<QString>> teacherLessonsMap;
        while (query.next()) {
            QString lessonId = query.value("LessonId").toString();
            QString teacherId = query.value("TeacherId").toString();
            QString tableName = "lesson_" + lessonId;
            // 检查对应的lesson_id表是否存在，如果不存在则创建
            if (!ifTableExist(tableName)) {
                Status status = createTableIfNotExists(tableName);
                if (status != Success) {
                    db.rollback();
                    return status;
                }
            }

            // 记录每个教师的授课信息
            teacherLessonsMap[teacherId].append(lessonId);
        }

        // 更新每个教师的授课信息
        for (auto it = teacherLessonsMap.begin(); it != teacherLessonsMap.end(); ++it) {
            Status status = updateTeachingLessons(it.key(), it.value());
            if (status != Success) {
                db.rollback();
                return status;
            }
        }

        // 检查每个学生的选课信息是否正确
        query.prepare("SELECT StudentId, ChosenLessons FROM student_information");
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: checkDatabase error: " << query.lastError();
            db.rollback();
            return ERROR;
        }
        while (query.next()) {
            QString studentId = query.value("StudentId").toString();
            QString chosenLessonsJson = query.value("ChosenLessons").toString();
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(chosenLessonsJson.toUtf8(), &jsonError);
            QJsonArray array = doc.array();
            QVector<QString> chosenLessons;
            for (auto &&i: array) {
                chosenLessons.append(i.toString());
            }
            // 检查学生的选课信息是否正确，如果不正确则修正
            for (const auto &lessonId: chosenLessons) {
                QString tableName = "lesson_" + lessonId;
                query.prepare("SELECT StudentId FROM " + tableName);
                if (!query.exec()) {
                    qDebug() << "Debug | database.cpp: checkDatabase error: " << query.lastError();
                    db.rollback();
                    return ERROR;
                }
                QVector<QString> lessonStudents;
                while (query.next()) {
                    lessonStudents.append(query.value("StudentId").toString());
                }
                if (!lessonStudents.contains(studentId)) {
                    // 如果学生的选课信息不正确，修正它
                    Status status = deleteChosenLesson(studentId, lessonId);
                    if (status != Success) {
                        db.rollback();
                        return status;
                    }
                    break;
                }
            }
        }

        // 提交事务
        db.commit();
        return Success;
    }

    Status database::deleteChosenLesson(const QString &studentId, const QString &lessonId) {
        QSqlQuery query;
        query.prepare("SELECT ChosenLessons FROM student_information WHERE StudentId = :studentId");
        query.bindValue(":studentId", studentId);
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: deleteChosenLesson error: " << query.lastError();
            return ERROR;
        }
        QString chosenLessonsJson = query.value("ChosenLessons").toString();
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(chosenLessonsJson.toUtf8(), &jsonError);
        QJsonArray array = doc.array();
        int index = -1;
        for (int i = 0; i < array.size(); i++) {
            if (array[i].toString() == lessonId) {
                index = i;
                break;
            }
        }
        if (index == -1) {
            return LESSON_NOT_FOUND;  // Lesson not found in the chosen lessons
        }
        array.removeAt(index);
        QJsonDocument newDoc(array);
        QString newChosenLessonsJson(newDoc.toJson(QJsonDocument::Compact));
        db.transaction();
        query.prepare("UPDATE student_information SET ChosenLessons = :chosenLessons WHERE StudentId = :studentId");
        query.bindValue(":chosenLessons", newChosenLessonsJson);
        query.bindValue(":studentId", studentId);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: deleteChosenLesson error: " << query.lastError();
            db.rollback();
            return ERROR;
        }
        db.commit();
        return Success;
    }

    Status database::updateStudent(const Student &student) {
        db.transaction();
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
            qDebug() << "Debug | database.cpp: updateStudent error: " << query.lastError();
            db.rollback();
            return ERROR;
        }
        db.commit();
        return Success;
    }

    Status database::updateLessonInformation(const Lesson &lesson) {
        db.transaction();
        QSqlQuery query;

        //检查教师是否存在
        Status status = ifTeacherExist(lesson.TeacherId);
        if (status != Success) {
            db.rollback();
            return status;
        }

        query.prepare(
                "INSERT OR REPLACE INTO lesson_information (LessonId, LessonName, TeacherId, LessonCredits, LessonSemester, LessonArea, LessonTimeAndLocations) "
                "VALUES (:id, :name, :teacherId, :credits, :area, :semester, :timeAndLocations)");
        query.bindValue(":id", lesson.Id);
        query.bindValue(":name", lesson.LessonName);
        query.bindValue(":teacherId", lesson.TeacherId);
        query.bindValue(":credits", lesson.LessonCredits);
        query.bindValue(":semester", lesson.LessonSemester);
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
            qDebug() << "Debug | database.cpp: updateLessonInformation error: " << query.lastError();
            db.rollback();
            return ERROR;
        }
        QString tableName = "lesson_" + lesson.Id;
        status = createTableIfNotExists(tableName);
        if (status != Success) {
            db.rollback();
            return status;
        }
        //更新老师的教课信息
        status = addTeachingLesson(lesson.TeacherId, lesson.Id);
        if (status != Success) {
            db.rollback();
            return status;
        }
        db.commit();
        return Success;
    }

    Status database::ifTeacherExist(const QString &teacherId) {
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM teacher_information WHERE TeacherId = :id");
        query.bindValue(":id", teacherId);
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: ifTeacherExist error: " << query.lastError();
            return ERROR;
        }
        int count = query.value(0).toInt();
        if (count == 0) {
            return TEACHER_NOT_FOUND;
        }
        return Success;
    }

    Status database::createTableIfNotExists(const QString &tableName) {
        db.transaction();
        QSqlQuery query;
        if (!ifTableExist(tableName)) {
            // 如果不存在，创建新表
            qDebug() << "Debug | database.cpp: 正在创建表" << tableName;
            query.prepare(R"(CREATE TABLE IF NOT EXISTS ")" + tableName + R"(" (
            "StudentId" TEXT NOT NULL UNIQUE,
            "ExamGrade" REAL,
            "RegularGrade" REAL,
            "TotalGrade" REAL,
            "Retake" INTEGER CHECK(Retake in (0, 1, 2)) DEFAULT 0,
            "RetakeSemesters" TEXT NOT NULL DEFAULT '[]',
            "RetakeLessonId" TEXT NOT NULL DEFAULT '[]',
            PRIMARY KEY("StudentId"),
            FOREIGN KEY ("StudentId") REFERENCES "student_information"("StudentId")
            ON UPDATE NO ACTION ON DELETE NO ACTION
        )
    )");
            if (!query.exec()) {
                qDebug() << "Debug | database.cpp: Error creating table" << tableName << ":" << query.lastError();
                db.rollback();
                return ERROR;
            }
            qDebug() << "Debug | database.cpp: 表" << tableName << "创建成功";
        }
        db.commit();
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
            lesson.LessonSemester = record.value("LessonSemester").toString();
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
        db.transaction();
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
            qDebug() << "Debug | database.cpp: updateTeachingLessons error:" << query.lastError();
            db.rollback();
            return ERROR;
        }
        db.commit();
        return Success;
    }

    Status database::addTeachingLesson(const QString &teacherId, const QString &lessonId) {
        QSqlQuery query;
        query.prepare("SELECT TeachingLessons FROM teacher_information WHERE TeacherId = :teacherId");
        query.bindValue(":teacherId", teacherId);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: addTeachingLesson error:" << query.lastError();
            return ERROR;
        }
        if (!query.next()) {
            return TEACHER_NOT_FOUND;
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
        db.transaction();
        query.prepare("UPDATE teacher_information SET TeachingLessons = :teachingLessons WHERE TeacherId = :teacherId");
        query.bindValue(":teachingLessons", newTeachingLessonsJson);
        query.bindValue(":teacherId", teacherId);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: addTeachingLesson error:" << query.lastError();
            db.rollback();
            return ERROR;
        }
        db.commit();
        return Success;
    }

    Status database::deleteTeachingLesson(const QString &teacherId, const QString &lessonId) {
        QSqlQuery query;
        query.prepare("SELECT TeachingLessons FROM teacher_information WHERE TeacherId = :teacherId");
        query.bindValue(":teacherId", teacherId);
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: deleteTeachingLesson error:" << query.lastError();
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
        db.transaction();
        query.prepare("UPDATE teacher_information SET TeachingLessons = :teachingLessons WHERE TeacherId = :teacherId");
        query.bindValue(":teachingLessons", newTeachingLessonsJson);
        query.bindValue(":teacherId", teacherId);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: deleteTeachingLesson error:" << query.lastError();
            db.rollback();
            return ERROR;
        }
        db.commit();
        return Success;
    }

    Status database::getTeacherById(const QString &id, Teacher &teacher) {
        QSqlQuery query;
        query.prepare("SELECT * FROM teacher_information WHERE TeacherId = :id");
        query.bindValue(":id", id);
        if (query.exec() && query.next()) {
            QSqlRecord record = query.record();
            teacher.Id = record.value("TeacherId").toString();
            teacher.Name = record.value("TeacherName").toString();
            teacher.Uint = record.value("TeacherUint").toString();

            QString teachingLessonsJson = record.value("TeachingLessons").toString();
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(teachingLessonsJson.toUtf8(), &jsonError);
            QJsonArray array = doc.array();
            teacher.TeachingLessons.clear();
            for (auto &&i: array) {
                teacher.TeachingLessons.append(i.toString());
            }
            return Success;
        }
        return ERROR;
    }

    Status database::updateTeacher(const Teacher &teacher) {
        db.transaction();
        QSqlQuery query;
        query.prepare(
                "INSERT OR REPLACE INTO teacher_information (TeacherId, TeacherName, TeacherUnit) VALUES (:id, :name :unit)");
        query.bindValue(":id", teacher.Id);
        query.bindValue(":name", teacher.Name);
        query.bindValue(":unit", teacher.Uint);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: updateTeacher error:" << query.lastError();
            db.rollback();
            return ERROR;
        }
        db.commit();
        return Success;
    }

    Status database::deleteStudent(const QString &id) {
        QSqlQuery query;

        // Check if the student exists
        query.prepare("SELECT COUNT(*) FROM student_information WHERE StudentId = :id");
        query.bindValue(":id", id);
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: deleteStudent error:" << query.lastError();
            return ERROR;
        }
        int count = query.value(0).toInt();
        if (count == 0) {
            return STUDENT_NOT_FOUND;
        }
        // 获取学生的已选课程
        query.prepare("SELECT ChosenLessons FROM student_information WHERE StudentId = :id");
        query.bindValue(":id", id);
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: deleteStudent error:" << query.lastError();
            return ERROR;
        }
        QString chosenLessonsJson = query.value("ChosenLessons").toString();
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(chosenLessonsJson.toUtf8(), &jsonError);
        QJsonArray array = doc.array();
        QVector<QString> chosenLessons;
        for (auto &&i: array) {
            chosenLessons.append(i.toString());
        }

        db.transaction();
        // 从每个已选课程中删除该学生
        for (const auto &lessonId: chosenLessons) {
            query.prepare("SELECT LessonStudents FROM lesson_information WHERE LessonId = :lessonId");
            query.bindValue(":lessonId", lessonId);
            if (!query.exec() || !query.next()) {
                qDebug() << "Debug | database.cpp: deleteStudent error:" << query.lastError();
                db.rollback();
                return ERROR;
            }
            QString lessonStudentsJson = query.value("LessonStudents").toString();
            doc = QJsonDocument::fromJson(lessonStudentsJson.toUtf8(), &jsonError);
            array = doc.array();
            for (int i = 0; i < array.size(); i++) {
                if (array[i].toString() == id) {
                    array.removeAt(i);
                    break;
                }
            }
            QJsonDocument newDoc(array);
            QString newLessonStudentsJson(newDoc.toJson(QJsonDocument::Compact));
            query.prepare("UPDATE lesson_information SET LessonStudents = :lessonStudents WHERE LessonId = :lessonId");
            query.bindValue(":lessonStudents", newLessonStudentsJson);
            query.bindValue(":lessonId", lessonId);
            if (!query.exec()) {
                qDebug() << "Debug | database.cpp: deleteStudent error:" << query.lastError();
                db.rollback();
                return ERROR;
            }

            // 从每个课程对应的 lesson_id 表中删除该学生
            QString tableName = "lesson_" + lessonId;
            query.prepare("DELETE FROM " + tableName + " WHERE StudentId = :id");
            query.bindValue(":id", id);
            if (!query.exec()) {
                qDebug() << "Debug | database.cpp: deleteStudent error:" << query.lastError();
                db.rollback();
                return ERROR;
            }
        }

        // 删除学生主记录
        query.prepare("DELETE FROM student_information WHERE StudentId = :id");
        query.bindValue(":id", id);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: deleteStudent error:" << query.lastError();
            db.rollback();
            return ERROR;
        }
        db.commit();
        return Success;
    }

    Status database::deleteLesson(const QString &id) {
        QSqlQuery query;
        // 获取课程的学生列表
        query.prepare("SELECT LessonStudents FROM lesson_information WHERE LessonId = :id");
        query.bindValue(":id", id);
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: deleteLesson error:" << query.lastError();
            return ERROR;
        }
        QString lessonStudentsJson = query.value("LessonStudents").toString();
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(lessonStudentsJson.toUtf8(), &jsonError);
        QJsonArray array = doc.array();
        QVector<QString> lessonStudents;
        for (auto &&i: array) {
            lessonStudents.append(i.toString());
        }

        // 从每个学生的已选课程中删除该课程
        db.transaction();
        for (const auto &studentId: lessonStudents) {
            query.prepare("SELECT ChosenLessons FROM student_information WHERE StudentId = :studentId");
            query.bindValue(":studentId", studentId);
            if (!query.exec() || !query.next()) {
                qDebug() << "Debug | database.cpp: deleteLesson error:" << query.lastError();
                db.rollback();
                return ERROR;
            }
            QString chosenLessonsJson = query.value("ChosenLessons").toString();
            doc = QJsonDocument::fromJson(chosenLessonsJson.toUtf8(), &jsonError);
            array = doc.array();
            for (int i = 0; i < array.size(); i++) {
                if (array[i].toString() == id) {
                    array.removeAt(i);
                    break;
                }
            }
            QJsonDocument newDoc(array);
            QString newChosenLessonsJson(newDoc.toJson(QJsonDocument::Compact));
            query.prepare("UPDATE student_information SET ChosenLessons = :chosenLessons WHERE StudentId = :studentId");
            query.bindValue(":chosenLessons", newChosenLessonsJson);
            query.bindValue(":studentId", studentId);
            if (!query.exec()) {
                qDebug() << "Debug | database.cpp: deleteLesson error:" << query.lastError();
                db.rollback();
                return ERROR;
            }
        }

        // 删除老师该课程的教课信息
        Status status = deleteTeachingLesson(query.value("TeacherId").toString(), id);
        if (status != Success) {
            return status;
        }

        // 删除课程主记录
        query.prepare("DELETE FROM lesson_information WHERE LessonId = :id");
        query.bindValue(":id", id);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: deleteLesson error:" << query.lastError();
            db.rollback();
            return ERROR;
        }


        // 删除课程对应的 lesson_id 表
        QString tableName = "lesson_" + id;
        query.prepare("DROP TABLE IF EXISTS " + tableName);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: deleteLesson error:" << query.lastError();
            db.rollback();
            return ERROR;
        }
        db.commit();
        return Success;
    }

    Status database::deleteTeacher(const QString &id) {
        QSqlQuery query;
        // 获取老师的教课信息
        query.prepare("SELECT TeachingLessons FROM teacher_information WHERE TeacherId = :id");
        query.bindValue(":id", id);
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: deleteTeacher error:" << query.lastError();
            return ERROR;
        }
        QString teachingLessonsJson = query.value("TeachingLessons").toString();
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(teachingLessonsJson.toUtf8(), &jsonError);
        QJsonArray array = doc.array();
        QVector<QString> teachingLessons;
        for (auto &&i: array) {
            teachingLessons.append(i.toString());
        }

        // 如果老师正在任教的课程不为空，返回错误
        if (!teachingLessons.isEmpty()) {
            qDebug() << "Debug | database.cpp: deleteTeacher error: Teacher is still teaching courses";
            return RELATION_ERROR;
        }

        // 删除老师主记录
        db.transaction();
        query.prepare("DELETE FROM teacher_information WHERE TeacherId = :id");
        query.bindValue(":id", id);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: deleteTeacher error:" << query.lastError();
            db.rollback();
            return ERROR;
        }
        db.commit();
        return Success;
    }

    Status database::getStudentByClass(const QString &studentClass, QVector<Student> &students) {
        QSqlQuery query;
        query.prepare("SELECT * FROM student_information WHERE StudentClass = :studentClass");
        query.bindValue(":studentClass", studentClass);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: getStudentByClass error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            QSqlRecord record = query.record();
            Student student;
            student.Id = record.value("StudentId").toString();
            student.Name = record.value("StudentName").toString();
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
            for (auto &&i: array) {
                student.ChosenLessons.append(i.toString());
            }
            students.append(student);
        }
        return Success;
    }

    Status database::listStudents(QVector<Student> &students, int maximum, int pageNum) {
        QSqlQuery query;
        query.prepare("SELECT * FROM student_information LIMIT :maximum OFFSET :offset");
        query.bindValue(":maximum", maximum);
        query.bindValue(":offset", maximum * (pageNum - 1));
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: listStudents error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            QSqlRecord record = query.record();
            Student student;
            student.Id = record.value("StudentId").toString();
            student.Name = record.value("StudentName").toString();
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
            for (auto &&i: array) {
                student.ChosenLessons.append(i.toString());
            }
            students.append(student);
        }
        return Success;
    }

    int database::getStudentCount() {
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM student_information");
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: getStudentCount error:" << query.lastError();
            return -1;
        }
        return query.value(0).toInt();
    }

    int database::getLessonCount() {
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM lesson_information");
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: getLessonCount error:" << query.lastError();
            return -1;
        }
        return query.value(0).toInt();
    }

    int database::getTeacherCount() {
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM teacher_information");
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: getTeacherCount error:" << query.lastError();
            return -1;
        }
        return query.value(0).toInt();
    }

    int database::getAuthCount() {
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM auth");
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: getAuthCount error:" << query.lastError();
            return -1;
        }
        return query.value(0).toInt();
    }

    Status database::listLessons(QVector<Lesson> &lessons, int maximum, int pageNum) {
        QSqlQuery query;
        query.prepare("SELECT * FROM lesson_information LIMIT :maximum OFFSET :offset");
        query.bindValue(":maximum", maximum);
        query.bindValue(":offset", maximum * (pageNum - 1));
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: listLessons error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            QSqlRecord record = query.record();
            Lesson lesson;
            lesson.Id = record.value("LessonId").toString();
            lesson.LessonName = record.value("LessonName").toString();
            lesson.TeacherId = record.value("TeacherId").toString();
            lesson.LessonCredits = record.value("LessonCredits").toInt();
            lesson.LessonSemester = record.value("LessonSemester").toString();
            lesson.LessonArea = record.value("LessonArea").toString();

            QString lessonTimeAndLocationsJson = record.value("LessonTimeAndLocations").toString();
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(lessonTimeAndLocationsJson.toUtf8(), &jsonError);
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
            for (auto &&i: array) {
                lesson.LessonStudents.append(i.toString());
            }
            lessons.append(lesson);
        }
        return Success;
    }

    Status database::listTeachers(QVector<Teacher> &teachers, int maximum, int pageNum) {
        QSqlQuery query;
        query.prepare("SELECT * FROM teacher_information LIMIT :maximum OFFSET :offset");
        query.bindValue(":maximum", maximum);
        query.bindValue(":offset", maximum * (pageNum - 1));
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: listTeachers error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            QSqlRecord record = query.record();
            Teacher teacher;
            teacher.Id = record.value("TeacherId").toString();
            teacher.Name = record.value("TeacherName").toString();
            teacher.Uint = record.value("TeacherUint").toString();

            QString teachingLessonsJson = record.value("TeachingLessons").toString();
            QJsonParseError jsonError;
            QJsonDocument doc = QJsonDocument::fromJson(teachingLessonsJson.toUtf8(), &jsonError);
            QJsonArray array = doc.array();
            for (auto &&i: array) {
                teacher.TeachingLessons.append(i.toString());
            }
            teachers.append(teacher);
        }
        return Success;
    }

    Status database::listAuths(QVector<Auth> &auths, int maximum, int pageNum) {
        QSqlQuery query;
        query.prepare("SELECT * FROM auth LIMIT :maximum OFFSET :offset");
        query.bindValue(":maximum", maximum);
        query.bindValue(":offset", maximum * (pageNum - 1));
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: listAuths error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            QSqlRecord record = query.record();
            Auth auth;
            auth.Account = record.value("Account").toString();
            auth.Secret = record.value("Secret").toString();
            auth.AccountType = record.value("AccountType").toInt();
            auth.IsSuper = record.value("IsSuper").toInt();
            auths.append(auth);
        }
        return Success;
    }

    Status database::createAccount(const Auth &auth) {
        QSqlQuery query;

        // Check if the account already exists
        query.prepare("SELECT COUNT(*) FROM auth WHERE Account = :account");
        query.bindValue(":account", auth.Account);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: createAccount error:" << query.lastError();
            return ERROR;
        }
        if (query.next() && query.value(0).toInt() > 0) {
            return DUPLICATE;
        }

        // If the account does not exist, create it
        query.prepare(
                "INSERT INTO auth (Account, Secret, AccountType, IsSuper) VALUES (:account, :secret, :accountType, :isSuper)");
        query.bindValue(":account", auth.Account);
        query.bindValue(":secret", auth.Secret);
        query.bindValue(":accountType", auth.AccountType);
        query.bindValue(":isSuper", auth.IsSuper);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: createAccount error:" << query.lastError();
            return ERROR;
        }
        return Success;
    }

    Status database::updateAccount(const Auth &auth) {
        QSqlQuery query;
        QString updateStatement = "UPDATE auth SET ";
        if (!auth.Secret.isEmpty()) {
            updateStatement += "Secret = :secret, ";
        }
        if (auth.AccountType != -1) {
            updateStatement += "AccountType = :accountType, ";
        }
        if (auth.IsSuper != -1) {
            updateStatement += "IsSuper = :isSuper, ";
        }
        // Remove the last comma and space
        updateStatement = updateStatement.left(updateStatement.length() - 2);
        updateStatement += " WHERE Account = :account";
        query.prepare(updateStatement);
        query.bindValue(":account", auth.Account);
        if (!auth.Secret.isEmpty()) {
            query.bindValue(":secret", auth.Secret);
        }
        if (auth.AccountType != -1) {
            query.bindValue(":accountType", auth.AccountType);
        }
        if (auth.IsSuper != -1) {
            query.bindValue(":isSuper", auth.IsSuper);
        }
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: updateAccount error:" << query.lastError();
            return ERROR;
        }
        return Success;
    }

    Status database::deleteAccount(const QString &account) {
        QSqlQuery query;
        query.prepare("DELETE FROM auth WHERE Account = :account");
        query.bindValue(":account", account);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: deleteAccount error:" << query.lastError();
            return ERROR;
        }
        return Success;
    }

    Status database::getAccount(const QString &account, Auth &auth) {
        QSqlQuery query;
        query.prepare("SELECT * FROM auth WHERE Account = :account");
        query.bindValue(":account", account);
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: getAccount error:" << query.lastError();
            return ERROR;
        }
        if (!query.next()) {
            return NOT_FOUND;
        }
        QSqlRecord record = query.record();
        auth.Account = record.value("Account").toString();
        auth.Secret = record.value("Secret").toString();
        auth.AccountType = record.value("AccountType").toInt();
        auth.IsSuper = record.value("IsSuper").toInt();
        return Success;
    }

    Status database::verifyAccount(const QString &account, const QString &secret, Auth &auth) {
        QSqlQuery query;
        query.prepare("SELECT * FROM auth WHERE Account = :account");
        query.bindValue(":account", account);
        if (!query.exec()) {
            return ERROR;
        }
        if (!query.next()) {
            return NOT_FOUND;
        }
        QSqlRecord record = query.record();
        if (record.value("Secret").toString() != secret) {
            return INVALID;
        }
        auth.Account = record.value("Account").toString();
        auth.AccountType = record.value("AccountType").toInt();
        auth.IsSuper = record.value("IsSuper").toInt();
        return Success;
    }

    Status database::listClass(QVector<QString> &classes) {
        QSqlQuery query;
        query.prepare("SELECT DISTINCT StudentClass FROM student_information");
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: listClass error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            classes.append(query.value(0).toString());
        }
        return Success;
    }

    Status database::listCollege(QVector<QString> &colleges) {
        QSqlQuery query;
        query.prepare("SELECT DISTINCT StudentCollege FROM student_information");
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: listCollege error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            colleges.append(query.value(0).toString());
        }
        return Success;
    }

    Status database::listMajor(QVector<QString> &majors) {
        QSqlQuery query;
        query.prepare("SELECT DISTINCT StudentMajor FROM student_information");
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: listMajor error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            majors.append(query.value(0).toString());
        }
        return Success;
    }

    Status database::listLessonArea(QVector<QString> &areas) {
        QSqlQuery query;
        query.prepare("SELECT DISTINCT LessonArea FROM lesson_information");
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: listLessonArea error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            areas.append(query.value(0).toString());
        }
        return Success;
    }

    Status database::listLessonSemester(QVector<QString> &semesters) {
        QSqlQuery query;
        query.prepare("SELECT DISTINCT LessonSemester FROM lesson_information");
        if (!query.exec()) {
            qDebug() << "Debug | database.cpp: listLessonSemester error:" << query.lastError();
            return ERROR;
        }
        while (query.next()) {
            semesters.append(query.value(0).toString());
        }
        return Success;
    }

    Status database::getStudentLessonGrade(const QString &studentId, const QString &lessonId, Grade &grade) {
        // 检查课程是否存在
        QSqlQuery lessonQuery;
        lessonQuery.prepare("SELECT COUNT(*) FROM lesson_information WHERE LessonId = :lessonId");
        lessonQuery.bindValue(":lessonId", lessonId);
        if (!lessonQuery.exec() || !lessonQuery.next() || lessonQuery.value(0).toInt() == 0) {
            qDebug() << "Debug | database.cpp: getStudentLessonGrade error: Lesson not found";
            return LESSON_NOT_FOUND;
        }

        // 检查学生是否存在
        QSqlQuery studentQuery;
        studentQuery.prepare("SELECT COUNT(*) FROM student_information WHERE StudentId = :studentId");
        studentQuery.bindValue(":studentId", studentId);
        if (!studentQuery.exec() || !studentQuery.next() || studentQuery.value(0).toInt() == 0) {
            qDebug() << "Debug | database.cpp: getStudentLessonGrade error: Student not found";
            return STUDENT_NOT_FOUND;
        }

        // 查询学生的课程成绩
        QSqlQuery query;
        query.prepare("SELECT * FROM lesson_" + lessonId + " WHERE StudentId = :studentId");
        query.bindValue(":studentId", studentId);
        if (!query.exec() || !query.next()) {
            qDebug() << "Debug | database.cpp: getStudentLessonGrade error:" << query.lastError();
            return ERROR;
        }
        QSqlRecord record = query.record();
        grade.StudentId = record.value("StudentId").toString();
        grade.ExamGrade = record.value("ExamGrade").toDouble();
        grade.RegularGrade = record.value("RegularGrade").toDouble();
        grade.TotalGrade = record.value("TotalGrade").toDouble();
        grade.Retake = record.value("Retake").toInt();
        QString retakeSemestersJson = record.value("RetakeSemesters").toString();
        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(retakeSemestersJson.toUtf8(), &jsonError);
        QJsonArray array = doc.array();
        for (auto &&i: array) {
            grade.RetakeSemesters.append(i.toString());
        }
        QString retakeLessonIdJson = record.value("RetakeLessonId").toString();
        doc = QJsonDocument::fromJson(retakeLessonIdJson.toUtf8(), &jsonError);
        array = doc.array();
        // grade.RetakeLessonId 是 QVector<QString> 类型
        for (auto &&id: array) {
            grade.RetakeLessonId.append(id.toString());
        }
        return Success;
    }


}// namespace Database
#pragma clang diagnostic pop