#include "database.h"
#include <QCoreApplication>
#include <QHttpServer>
#include <QCommandLineParser>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "jwt-cpp/jwt.h"

#define SCHEME "http"
#define HOST "127.0.0.1"
#define PORT 49425

#define SECRET_KEY "AIMS"
#define ISSUER "AIMS"

Auth verifyJwt(const QHttpServerRequest &request) {
    // 从header中获取JWT
    QString jwtString;
    const auto headers = request.headers();
    for (const auto &header: headers) {
        if (header.first == "Authorization") {
            jwtString = header.second;
            break;
        }
    }

    // 检查是否找到了"Authorization"头
    if (jwtString.isEmpty()) {
        // 如果没有找到，返回所有属性都为-1的Auth对象
        return Auth{"", "", -1, -1};
    }

    // 使用jwt-cpp库解码JWT
    auto decodedToken = jwt::decode(jwtString.toStdString());

    // 验证JWT
    auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{SECRET_KEY})
            .with_issuer(ISSUER);

    try {
        verifier.verify(decodedToken);
    } catch (const jwt::error::token_verification_exception &e) {
        // 如果验证失败，返回所有属性都为-1的Auth对象
        return Auth{"", "", -1, -1};
    }

    // 从JWT的payload中获取Auth信息
    Auth auth;
    auth.Account = QString::fromStdString(decodedToken.get_payload_claim("Account").as_string());
    auth.AccountType = static_cast<int>(decodedToken.get_payload_claim("AccountType").as_integer());
    auth.IsSuper = static_cast<int>(decodedToken.get_payload_claim("IsSuper").as_integer());

    // 返回Auth对象
    return auth;
}

//不验证具体Id
Status verifyAuth(const QHttpServerRequest &request, int accountType) {
    // 验证JWT
    Auth auth = verifyJwt(request);
    // 检查Auth对象的字段是否为空或者为-1
    if (auth.Account.isEmpty() || auth.AccountType == -1 || auth.IsSuper == -1) {
        // 如果Auth对象的字段为空或者为-1，返回登录失败的状态
        return INVALID;
    }
    // 检查账户类型是否匹配
    if (accountType == EVERYONE) {
        // 如果accountType为EVERYONE（3），则无需检查账户类型，直接返回成功状态
        return Success;
    } else if (accountType == SUPER) {
        // 如果accountType为SUPER（2），则需要检查IsSuper字段是否为1
        if (auth.IsSuper != 1) {
            // 如果IsSuper字段不为1，返回错误状态
            return NO_PERMISSION;
        }
    } else if (auth.AccountType != accountType) {
        // 如果账户类型不匹配，返回错误状态
        return NO_PERMISSION;
    }
    // 如果JWT验证成功并且账户类型匹配，返回成功状态
    return Success;
}

Status verifyAuth(const QHttpServerRequest &request, int accountType, const QString &id) {
    // 验证JWT
    Auth auth = verifyJwt(request);
    // 检查账户类型和ID是否匹配
    if (auth.AccountType != accountType || auth.Account != id) {
        // 如果账户类型或ID不匹配，返回错误状态
        return NO_PERMISSION;
    }
    // 如果JWT验证成功并且账户类型和ID匹配，返回成功状态
    return Success;
}

QString generateJwt(const QString &account, int accountType, int isSuper) {
    auto token = jwt::create()
            .set_issuer(ISSUER)
            .set_type("JWT")
            .set_payload_claim("Account", jwt::claim(account.toStdString()))
            .set_payload_claim("AccountType", jwt::claim(std::to_string(accountType)))
            .set_payload_claim("IsSuper", jwt::claim(std::to_string(isSuper)))
            .sign(jwt::algorithm::hs256{SECRET_KEY});

    return QString::fromStdString(token);
}

QHttpServerResponse login(const QHttpServerRequest &request, Database::database &database) {
    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 从QJsonObject中获取账号和密码
    QString account = jsonObject["Account"].toString();
    QString secret = jsonObject["Secret"].toString();

    // 调用verifyAccount函数，验证账号和密码
    Auth auth;
    Status status = database.verifyAccount(account, secret, auth);

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    if (status == Success) {
        // 如果验证成功，生成JWT
        QString jwt = generateJwt(account, auth.AccountType, auth.IsSuper);
        responseJsonObject["success"] = true;
        responseJsonObject["jwt"] = jwt;
    } else if (status == NOT_FOUND) {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Account not found";
    } else if (status == INVALID) {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Invalid password";
    } else {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Login failed";
    }
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);

    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

QHttpServerResponse createAccount(const QHttpServerRequest &request, Database::database &database) {
    // 验证JWT
    Status status = verifyAuth(request, SUPER);
    if (status != Success) {
        // 如果验证失败，返回错误信息
        QJsonObject responseJsonObject;
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "No permission";
        QJsonDocument responseDoc(responseJsonObject);
        QString responseString = responseDoc.toJson(QJsonDocument::Compact);
        QHttpServerResponse response("application/json", responseString.toUtf8());
        return response;
    }

    // 获取请求的body
    QByteArray body = request.body();

    // 解析body为一个QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject jsonObject = doc.object();

    // 从QJsonObject中获取账号、密码、账户类型和是否为超级用户
    Auth auth;
    auth.Account = jsonObject["Account"].toString();
    auth.Secret = jsonObject["Secret"].toString();
    auth.AccountType = jsonObject["AccountType"].toInt();
    auth.IsSuper = jsonObject["IsSuper"].toInt();

    // 调用createAccount函数，创建账号
    status = database.createAccount(auth);

    // 创建一个JSON响应
    QJsonObject responseJsonObject;
    if (status == Success) {
        responseJsonObject["success"] = true;
        responseJsonObject["message"] = "Account created successfully";
    } else if (status == DUPLICATE) {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Account already exists";
    } else {
        responseJsonObject["success"] = false;
        responseJsonObject["message"] = "Failed to create account";
    }
    QJsonDocument responseDoc(responseJsonObject);
    QString responseString = responseDoc.toJson(QJsonDocument::Compact);

    QHttpServerResponse response("application/json", responseString.toUtf8());
    return response;
}

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
    httpServer.route("/api/login/", QHttpServerRequest::Method::Post,
                     [&database](const QHttpServerRequest &request) {
                         return login(request, database);
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