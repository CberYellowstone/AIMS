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

void addRoute(QHttpServer &httpServer) {
    httpServer.route("/", []() {
        return "教务信息管理系统已运行！";
    });

    httpServer.route("/api/getStudentInformation/", [](const QString &studentId) {
        Student student;
        Status status = Database::database::getStudentById(studentId, student);
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
    });

    httpServer.route("/api/updateStudentInformation/", QHttpServerRequest::Method::Post,
                     [](const QHttpServerRequest &request) {
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
                         QJsonArray chosenLessonsArray = jsonObject["ChosenLessons"].toArray();
                         for (const auto &lesson: chosenLessonsArray) {
                             student.ChosenLessons.append(lesson.toString());
                         }

                         // 更新数据库
                         Status status = Database::database::updateStudent(student);

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
                     });
}


int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    Database::database database("AIMS.sqlite");

    quint16 portArg = PORT;
    QHttpServer httpServer;
    addRoute(httpServer);


    const auto port = httpServer.listen(QHostAddress::Any, portArg);
    if (!port) {
        qDebug() << QString("Server failed to listen on a port.");
        return 0;
    }

    qDebug() << QString("Running on http://127.0.0.1:%1/ (Press CTRL+C to quit)").arg(port);

    return QCoreApplication::exec();
}