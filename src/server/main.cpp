#include <QCoreApplication>
#include <QHttpServer>
#include "database.h"
#include <QCommandLineParser>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#define SCHEME "http"
#define HOST "127.0.0.1"
#define PORT 49425

QHttpServerResponse getStudentInformation(const QString &studentId, Database::database &database) {
    Student student;
    Status status = database.getStudentById(studentId, student);
    QJsonObject jsonObject;
    if (status == Success) {
        jsonObject["success"] = true;
        jsonObject["Id"] = student.Id;
        jsonObject["Name"] = student.Name;
        jsonObject["Sex"] = student.Sex;
        jsonObject["College"] = student.College;
        jsonObject["Major"] = student.Major;
        jsonObject["Class"] = student.Class;
        jsonObject["Age"] = student.Age;
        jsonObject["PhoneNumber"] = student.PhoneNumber;
        jsonObject["DormitoryArea"] = student.DormitoryArea;
        jsonObject["DormitoryNum"] = student.DormitoryNum;
        QJsonArray chosenLessonsArray;
        for (const auto &lesson: student.ChosenLessons) {
            chosenLessonsArray.append(lesson);
        }
        jsonObject["ChosenLessons"] = chosenLessonsArray;
    } else {
        jsonObject["success"] = false;
        jsonObject["message"] = "Failed to get student information";
    }
    QJsonDocument doc(jsonObject);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    // 创建一个JSON响应
    QHttpServerResponse response("application/json", jsonString.toUtf8());
    return response;
}

QHttpServerResponse updateStudentInformation(const QHttpServerRequest &request, Database::database &database) {
    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 从QJsonObject中获取学生的信息
    Student student;
    student.Id = jsonObject["Id"].toString();
    student.Name = jsonObject["Name"].toString();
    student.Sex = jsonObject["Sex"].toString();
    student.College = jsonObject["College"].toString();
    student.Major = jsonObject["Major"].toString();
    student.Class = jsonObject["Class"].toString();
    student.Age = jsonObject["Age"].toInt();
    student.PhoneNumber = jsonObject["PhoneNumber"].toString();
    student.DormitoryArea = jsonObject["DormitoryArea"].toString();
    student.DormitoryNum = jsonObject["DormitoryNum"].toString();

    // 更新数据库
    Status status = database.updateStudent(student);

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    if (status == Success) {
        responseJsonObject["success"] = true;
        responseJsonObject["message"] = "Student information updated successfully";
    } else {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Failed to update student information";
    }
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);

    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

QHttpServerResponse updateLessonInformation(const QHttpServerRequest &request, Database::database &database) {
    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 从QJsonObject中获取课程的信息
    Lesson lesson;
    lesson.Id = jsonObject["Id"].toString();
    lesson.LessonName = jsonObject["LessonName"].toString();
    lesson.TeacherId = jsonObject["TeacherId"].toString();
    lesson.LessonCredits = jsonObject["LessonCredits"].toInt();
    lesson.LessonArea = jsonObject["LessonArea"].toString();

    //jsonObject["LessonTimeAndLocations"]结构如下: {"1-6周":["40809节","4501"],"7-10周":["30609节","4601"]}
    QJsonObject lessonTimeAndLocations = jsonObject["LessonTimeAndLocations"].toObject();
    QMap<QString, QVector<QString>> timeAndLocationsMap;

    for (auto it = lessonTimeAndLocations.begin(); it != lessonTimeAndLocations.end(); ++it) {
        QJsonArray timeAndLocationArray = it.value().toArray();
        QVector<QString> timeAndLocation;
        for (auto &&i: timeAndLocationArray) {
            timeAndLocation.append(i.toString());
        }
        timeAndLocationsMap.insert(it.key(), timeAndLocation);
    }
    lesson.LessonTimeAndLocations = timeAndLocationsMap;


    QJsonArray lessonStudentsArray = jsonObject["LessonStudents"].toArray();
    for (const auto &lessonStudent: lessonStudentsArray) {
        lesson.LessonStudents.append(lessonStudent.toString());
    }

    // 更新数据库
    Status status = database.updateLessonInformation(lesson);

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    if (status == Success) {
        responseJsonObject["success"] = true;
        responseJsonObject["message"] = "Lesson information updated successfully";
    } else if (status == TEACHER_NOT_FOUND) {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Teacher not found";
    } else {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Failed to update lesson information";
    }
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);

    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

QHttpServerResponse getLessonInformation(const QString &lessonId, Database::database &database) {
    Lesson lesson;
    Status status = database.getLessonById(lessonId, lesson);
    QJsonObject jsonObject;
    if (status == Success) {
        jsonObject["success"] = true;
        jsonObject["Id"] = lesson.Id;
        jsonObject["LessonName"] = lesson.LessonName;
        jsonObject["TeacherId"] = lesson.TeacherId;
        jsonObject["LessonCredits"] = lesson.LessonCredits;
        jsonObject["LessonArea"] = lesson.LessonArea;

        QJsonObject lessonTimeAndLocationsObj;
        for (auto it = lesson.LessonTimeAndLocations.cbegin(); it != lesson.LessonTimeAndLocations.cend(); ++it) {
            QJsonArray jsonArray;
            for (const auto &str: it.value()) {
                jsonArray.append(QJsonValue(str));
            }
            lessonTimeAndLocationsObj.insert(it.key(), jsonArray);
        }
        jsonObject["LessonTimeAndLocations"] = lessonTimeAndLocationsObj;

        QJsonArray lessonStudentsArray;
        for (const auto &lessonStudent: lesson.LessonStudents) {
            lessonStudentsArray.append(lessonStudent);
        }
        jsonObject["LessonStudents"] = lessonStudentsArray;
    } else {
        jsonObject["success"] = false;
        jsonObject["message"] = "Failed to get lesson information";
    }
    QJsonDocument doc(jsonObject);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    // 创建一个JSON响应
    QHttpServerResponse response("application/json", jsonString.toUtf8());
    return response;
}

QHttpServerResponse getTeacherInformation(const QString &teacherId, Database::database &database) {
    Teacher teacher;
    Status status = database.getTeacherById(teacherId, teacher);
    QJsonObject jsonObject;
    if (status == Success) {
        jsonObject["success"] = true;
        jsonObject["Id"] = teacher.Id;
        jsonObject["Name"] = teacher.Name;
        QJsonArray teachingLessonsArray;
        for (const auto &teachingLesson: teacher.TeachingLessons) {
            teachingLessonsArray.append(teachingLesson);
        }
        jsonObject["TeachingLessons"] = teachingLessonsArray;
    } else {
        jsonObject["success"] = false;
        jsonObject["message"] = "Failed to get teacher information";
    }
    QJsonDocument doc(jsonObject);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    // 创建一个JSON响应
    QHttpServerResponse response("application/json", jsonString.toUtf8());
    return response;
}

QHttpServerResponse updateTeacherInformation(const QHttpServerRequest &request, Database::database &database) {
    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 从QJsonObject中获取教师的信息
    Teacher teacher;
    teacher.Id = jsonObject["Id"].toString();
    teacher.Name = jsonObject["Name"].toString();

    // 更新数据库
    Status status = database.updateTeacher(teacher);

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    if (status == Success) {
        responseJsonObject["success"] = true;
        responseJsonObject["message"] = "Teacher information updated successfully";
    } else {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Failed to update teacher information";
    }
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);

    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

QHttpServerResponse addTeachingLessons(const QHttpServerRequest &request, Database::database &database) {
    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 从QJsonObject中获取教师的编号和教授课程的编号
    QString teacherId = jsonObject["teacherId"].toString();
    QJsonArray teachingLessonsArray = jsonObject["teachingLessons"].toArray();
    // 调用addTeachingLesson函数
    for (const auto &teachingLesson: teachingLessonsArray) {
        Status status = database.addTeachingLesson(teacherId, teachingLesson.toString());
        if (status != Success) {
            // 创建一个JSON响应
            QJsonObject responseJsonObject;
            responseJsonObject["success"] = false;
            responseJsonObject["message"] = "Failed to add teaching lesson";
            QJsonDocument responseDoc(responseJsonObject);
            QString responseString = responseDoc.toJson(QJsonDocument::Compact);
            QHttpServerResponse response("application/json", responseString.toUtf8());
            return response;
        }
    }

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    responseJsonObject["success"] = true;
    responseJsonObject["message"] = "Teaching lessons added successfully";
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);
    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

QHttpServerResponse deleteStudentInformation(const QHttpServerRequest &request, Database::database &database) {
    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 从QJsonObject中获取学生的学号
    QString studentId = jsonObject["Id"].toString();

    // 调用deleteStudent函数
    Status status = database.deleteStudent(studentId);

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    if (status == Success) {
        responseJsonObject["success"] = true;
        responseJsonObject["message"] = "Student information deleted successfully";
    } else if (status == NOT_FOUND) {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Student not found";
    } else {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Failed to delete student information";
    }
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);

    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

QHttpServerResponse listStudents(const QHttpServerRequest &request, Database::database &database) {
    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 调用getStudentCount函数，获取学生总数
    int total = database.getStudentCount();

    // 从QJsonObject中获取Maximum关键字的值，如果不存在，则设置为学生总数
    int maximum = jsonObject.contains("Maximum") ? jsonObject["Maximum"].toInt() : total;

    // 从QJsonObject中获取page关键字的值，如果不存在，则设置为默认值1
    int page = jsonObject.contains("Page") ? jsonObject["Page"].toInt() : 1;

    //处理异常值，返回错误信息
    if (maximum <= 0 || page <= 0) {
        QJsonObject responseJsonObject;
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Invalid maximum or page";
        QJsonDocument responseDoc(responseJsonObject);
        QString responseString = responseDoc.toJson(QJsonDocument::Compact);
        QHttpServerResponse response("application/json", responseString.toUtf8());
        return response;
    }

    // 调用listStudents函数，获取指定页的学生列表
    QVector<Student> students;
    Status status = database.listStudents(students, maximum, page);

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    if (status == Success) {
        // 计算总页数
        int totalPages = (total + maximum - 1) / maximum;

        // 将学生列表、总页数和学生总数包装成一个JSON对象
        responseJsonObject["success"] = true;
        responseJsonObject["total"] = total;
        responseJsonObject["totalPages"] = totalPages;
        QJsonArray studentsArray;
        for (const auto &student: students) {
            QJsonObject studentObject;
            studentObject["Id"] = student.Id;
            studentObject["Name"] = student.Name;
            studentObject["Sex"] = student.Sex;
            studentObject["College"] = student.College;
            studentObject["Major"] = student.Major;
            studentObject["Class"] = student.Class;
            studentObject["Age"] = student.Age;
            studentObject["PhoneNumber"] = student.PhoneNumber;
            studentObject["DormitoryArea"] = student.DormitoryArea;
            studentObject["DormitoryNum"] = student.DormitoryNum;
            QJsonArray chosenLessonsArray;
            for (const auto &lesson: student.ChosenLessons) {
                chosenLessonsArray.append(lesson);
            }
            studentObject["ChosenLessons"] = chosenLessonsArray;
            studentsArray.append(studentObject);
        }
        responseJsonObject["students"] = studentsArray;
    } else {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Failed to list students";
    }
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);

    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

QHttpServerResponse listTeachers(const QHttpServerRequest &request, Database::database &database) {
    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 调用getTeacherCount函数，获取教师总数
    int total = database.getTeacherCount();

    // 从QJsonObject中获取Maximum关键字的值，如果不存在，则设置为教师总数
    int maximum = jsonObject.contains("Maximum") ? jsonObject["Maximum"].toInt() : total;

    // 从QJsonObject中获取page关键字的值，如果不存在，则设置为默认值1
    int page = jsonObject.contains("Page") ? jsonObject["Page"].toInt() : 1;

    //处理异常值，返回错误信息
    if (maximum <= 0 || page <= 0) {
        QJsonObject responseJsonObject;
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Invalid maximum or page";
        QJsonDocument responseDoc(responseJsonObject);
        QString responseString = responseDoc.toJson(QJsonDocument::Compact);
        QHttpServerResponse response("application/json", responseString.toUtf8());
        return response;
    }

    // 调用listTeachers函数，获取指定页的教师列表
    QVector<Teacher> teachers;
    Status status = database.listTeachers(teachers, maximum, page);

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    if (status == Success) {
        // 计算总页数
        int totalPages = (total + maximum - 1) / maximum;

        // 将教师列表、总页数和教师总数包装成一个JSON对象
        responseJsonObject["success"] = true;
        responseJsonObject["total"] = total;
        responseJsonObject["totalPages"] = totalPages;
        QJsonArray teachersArray;
        for (const auto &teacher: teachers) {
            QJsonObject teacherObject;
            teacherObject["Id"] = teacher.Id;
            teacherObject["Name"] = teacher.Name;
            QJsonArray teachingLessonsArray;
            for (const auto &lesson: teacher.TeachingLessons) {
                teachingLessonsArray.append(lesson);
            }
            teacherObject["TeachingLessons"] = teachingLessonsArray;
            teachersArray.append(teacherObject);
        }
        responseJsonObject["teachers"] = teachersArray;
    } else {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Failed to list teachers";
    }
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);

    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

QHttpServerResponse listLessons(const QHttpServerRequest &request, Database::database &database) {
    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 调用getLessonCount函数，获取课程总数
    int total = database.getLessonCount();

    // 从QJsonObject中获取Maximum关键字的值，如果不存在，则设置为课程总数
    int maximum = jsonObject.contains("Maximum") ? jsonObject["Maximum"].toInt() : total;

    // 从QJsonObject中获取page关键字的值，如果不存在，则设置为默认值1
    int page = jsonObject.contains("Page") ? jsonObject["Page"].toInt() : 1;

    //处理异常值，返回错误信息
    if (maximum <= 0 || page <= 0) {
        QJsonObject responseJsonObject;
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Invalid maximum or page";
        QJsonDocument responseDoc(responseJsonObject);
        QString responseString = responseDoc.toJson(QJsonDocument::Compact);
        QHttpServerResponse response("application/json", responseString.toUtf8());
        return response;
    }

    // 调用listLessons函数，获取指定页的课程列表
    QVector<Lesson> lessons;
    Status status = database.listLessons(lessons, maximum, page);

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    if (status == Success) {
        // 计算总页数
        int totalPages = (total + maximum - 1) / maximum;

        // 将课程列表、总页数和课程总数包装成一个JSON对象
        responseJsonObject["success"] = true;
        responseJsonObject["total"] = total;
        responseJsonObject["totalPages"] = totalPages;
        QJsonArray lessonsArray;
        for (const auto &lesson: lessons) {
            QJsonObject lessonObject;
            lessonObject["Id"] = lesson.Id;
            lessonObject["LessonName"] = lesson.LessonName;
            lessonObject["TeacherId"] = lesson.TeacherId;
            lessonObject["LessonCredits"] = lesson.LessonCredits;
            lessonObject["LessonSemester"] = lesson.LessonSemester;
            lessonObject["LessonArea"] = lesson.LessonArea;

            QJsonObject lessonTimeAndLocationsObj;
            for (auto it = lesson.LessonTimeAndLocations.cbegin(); it != lesson.LessonTimeAndLocations.cend(); ++it) {
                QJsonArray jsonArray;
                for (const auto &str: it.value()) {
                    jsonArray.append(QJsonValue(str));
                }
                lessonTimeAndLocationsObj.insert(it.key(), jsonArray);
            }
            lessonObject["LessonTimeAndLocations"] = lessonTimeAndLocationsObj;

            QJsonArray lessonStudentsArray;
            for (const auto &lessonStudent: lesson.LessonStudents) {
                lessonStudentsArray.append(lessonStudent);
            }
            lessonObject["LessonStudents"] = lessonStudentsArray;

            lessonsArray.append(lessonObject);
        }
        responseJsonObject["lessons"] = lessonsArray;
    } else {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Failed to list lessons";
    }
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);

    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

void addRoute(QHttpServer &httpServer, Database::database database) {
    httpServer.route("/", []() {
        return "教务信息管理系统已运行！";
    });
    httpServer.route("/api/getStudentInformation/", [&database](const QString &studentId) {
        return getStudentInformation(studentId, database);
    });
    httpServer.route("/api/updateStudentInformation/", QHttpServerRequest::Method::Post,
                     [&database](const QHttpServerRequest &request) {
                         return updateStudentInformation(request, database);
                     });
    httpServer.route("/api/updateLessonInformation/", QHttpServerRequest::Method::Post,
                     [&database](const QHttpServerRequest &request) {
                         return updateLessonInformation(request, database);
                     });
    httpServer.route("/api/getLessonInformation/", [&database](const QString &lessonId) {
        return getLessonInformation(lessonId, database);
    });
    httpServer.route("/api/getTeacherInformation/", [&database](const QString &teacherId) {
        return getTeacherInformation(teacherId, database);
    });
    httpServer.route("/api/updateTeacherInformation/", QHttpServerRequest::Method::Post,
                     [&database](const QHttpServerRequest &request) {
                         return updateTeacherInformation(request, database);
                     });
    httpServer.route("/api/addTeachingLessons/", QHttpServerRequest::Method::Post,
                     [&database](const QHttpServerRequest &request) {
                         return addTeachingLessons(request, database);
                     });
    httpServer.route("/api/deleteStudentInformation/", QHttpServerRequest::Method::Post,
                     [&database](const QHttpServerRequest &request) {
                         return deleteStudentInformation(request, database);
                     });
    httpServer.route("/api/listStudents/", QHttpServerRequest::Method::Post,
                     [&database](const QHttpServerRequest &request) {
                         return listStudents(request, database);
                     });
    httpServer.route("/api/listTeachers/", QHttpServerRequest::Method::Post,
                     [&database](const QHttpServerRequest &request) {
                         return listTeachers(request, database);
                     });
    httpServer.route("/api/listLessons/", QHttpServerRequest::Method::Post,
                     [&database](const QHttpServerRequest &request) {
                         return listLessons(request, database);
                     });
}


int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    Database::database database("AIMS.sqlite");

    quint16 portArg = PORT;
    QHttpServer httpServer;
    addRoute(httpServer, database);

    const auto port = httpServer.listen(QHostAddress::Any, portArg);
    if (!port) {
        qDebug() << QString("Server failed to listen on a port.");
        return 0;
    }

    qDebug() << QString("Running on http://127.0.0.1:%1/ (Press CTRL+C to quit)").arg(port);

    return QCoreApplication::exec();
}