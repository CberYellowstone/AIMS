#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QJsonArray>
#include "ui_AIMSMainWindow.h"
#include "ui_LoginForm.h"
#include "../server/database.h"

#define SALT "AIMS"

class LoginForm : public QWidget, public Ui::LoginForm {
Q_OBJECT

public:
    explicit LoginForm(QWidget *parent = nullptr) : QWidget(parent) {
        setupUi(this);
        lineEdit_ServerURL->hide();
    }

public slots:

    void switchLineEdit_ServerURL() {
        if (lineEdit_ServerURL->isHidden()) {
            lineEdit_ServerURL->show();
        } else {
            lineEdit_ServerURL->hide();
        }
    }

    void login() {
        // ONLY FOR DEBUG
        lineEdit_Account->setText("202325310229");
        lineEdit_Password->setText("1");

        if (lineEdit_Account->text().isEmpty() || lineEdit_Password->text().isEmpty()) {
            QMessageBox::warning(this, "警告", "账号或密码不能为空");
            return;
        }

        if (lineEdit_ServerURL->text().isEmpty()) {
            QMessageBox::warning(this, "警告", "服务器地址不能为空");
            return;
        }

        QString account = lineEdit_Account->text();
        // secret = md5(md5(password) + SALT)
        QString secret = QCryptographicHash::hash(
                QCryptographicHash::hash(lineEdit_Password->text().toUtf8(), QCryptographicHash::Md5).toHex() + SALT,
                QCryptographicHash::Md5).toHex();
        // qDebug() << "account" << account << "secret" << secret;
        // Post请求
        QString serverURL = lineEdit_ServerURL->text() + "/api/login/";

        // 服务器端验证账号密码
        QNetworkRequest request;
        request.setUrl(QUrl(serverURL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject json;
        json.insert("Account", account);
        json.insert("Secret", secret);

        QJsonDocument doc(json);
        QByteArray data = doc.toJson();

        QNetworkAccessManager manager;
        QNetworkReply *reply = manager.post(request, data);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(3000);  // 5 seconds timeout
        loop.exec();

        if (timer.isActive()) {
            // The request finished within 3 seconds
            timer.stop();
            if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ContentNotFoundError &&
                reply->error() != QNetworkReply::ProtocolInvalidOperationError) {
                if (reply->error() == QNetworkReply::TimeoutError) {
                    QMessageBox::warning(this, "警告", "请求超时");
                } else {
                    QMessageBox::warning(this, "警告", "请求失败：" + QVariant::fromValue(reply->error()).toString());
                }
                return;
            }

            QByteArray responseData = reply->readAll();
            doc = QJsonDocument::fromJson(responseData);
            json = doc.object();

            if (json["success"].toBool()) {
                QString jwt = json["jwt"].toString();
                emit LoginSuccess(jwt);
                close();
            } else {
                QString message = json["message"].toString();
                QMessageBox::warning(this, "警告", "登录失败：" + message);
            }
        } else {
            // The request did not finish within 5 seconds
            disconnect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            reply->abort();
            reply->deleteLater();
            QMessageBox::warning(this, "警告", "请求超时");
        }
    }

private:

signals:

    void LoginSuccess(QString);
};

class AIMSMainWindow : public QMainWindow, public Ui::AIMSMainWindow {
Q_OBJECT  // 使用Q_OBJECT宏，以支持信号和槽

public:
    bool isLogin = false;
    LoginForm loginForm;
    QString JWT;
    QString serverURL;
    Student currentStudent;
    QVector<Lesson> chosenLessons;
    QMap<QString, Teacher> localTeachersTemp;
    QMap<QString, Lesson> localLessonsTemp;
    QString currentSemester;
    QMap<QPair<QString, QString>, Grade> localGradesTemp;

    explicit AIMSMainWindow(QMainWindow *parent = nullptr) : QMainWindow(parent) {
        setupUi(this);
        calculateCurrentSemester();
        connect(&loginForm, &LoginForm::LoginSuccess, this, &AIMSMainWindow::onLoginSuccess);
    }

    void calculateCurrentSemester() {
        //学期组成: 起始年份-结束年份-学期
        //学期和月份的对应关系:9-2->1, 3-8->2

        QDateTime currentDateTime = QDateTime::currentDateTime();
        int currentYear = currentDateTime.date().year();
        int currentMonth = currentDateTime.date().month();
        //计算当前学期
        if (currentMonth >= 9 && currentMonth <= 12) {
            currentSemester = QString::number(currentYear) + "-" + QString::number(currentYear + 1) + "-1";
        } else if (currentMonth >= 3 && currentMonth <= 8) {
            currentSemester = QString::number(currentYear - 1) + "-" + QString::number(currentYear) + "-2";
        } else if (currentMonth >= 1 && currentMonth <= 2) {
            currentSemester = QString::number(currentYear - 1) + "-" + QString::number(currentYear) + "-1";
        }
        qDebug() << "currentSemester" << currentSemester;
    }

    void doLogin() {
        if (!isLogin) {
            loginForm.show();
        } else {
            show();
        }
    }

    static void decodeJWT(const QString &jwt, Auth &auth) {
        QStringList parts = jwt.split(".");
        if (parts.size() != 3) {
            return;
        }
        QByteArray payload = QByteArray::fromBase64(parts.at(1).toUtf8());
        QJsonDocument doc = QJsonDocument::fromJson(payload);
        QJsonObject json = doc.object();
        QString account = json["Account"].toString();
        int accountType = json["AccountType"].toString().toInt();
        int isSuper = json["IsSuper"].toString().toInt();
        auth.Account = account;
        auth.AccountType = accountType;
        auth.IsSuper = isSuper;
    }

    void updateButtons(int AccountType, int IsSuper) {
        if (AccountType == STUDENT) {
            pushButton_1->setText("当前课表");
            pushButton_2->setText("已选课程");
            pushButton_3->setText("我的成绩");
            pushButton_4->hide();
            pushButton_5->hide();
            pushButton_6->hide();
            connect(pushButton_1, &QPushButton::clicked, this, &AIMSMainWindow::switchStackedWidgetToStudent_Schedule);
            connect(pushButton_2, &QPushButton::clicked, this, &AIMSMainWindow::switchStackedWidgetToStudent_Chosen);
            connect(pushButton_3, &QPushButton::clicked, this, &AIMSMainWindow::switchStackedWidgetToStudent_Grade);
        } else if (IsSuper) {

        } else if (AccountType == TEACHER) {

        } else {
            QMessageBox::warning(this, "警告", "未知用户类型");
        }
    }

    void switchStackedWidgetToStudent_Schedule() {
        pushButton_1->setChecked(true);
        pushButton_2->setChecked(false);
        pushButton_3->setChecked(false);
        resizeTableWidget();
        stackedWidget->setCurrentIndex(1);
        updateChosenLessons();

        for (int i = 2022; i < 2025; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Schedule_Semester->addItem(semester);
            }
        }
        comboBox_Schedule_Semester->setCurrentText(currentSemester);
        fillTableWidget_Schedule();
    }

    void switchStackedWidgetToStudent_Chosen() {
        pushButton_1->setChecked(false);
        pushButton_2->setChecked(true);
        pushButton_3->setChecked(false);
        resizeTableWidget();
        stackedWidget->setCurrentIndex(2);
        updateChosenLessons();

        for (int i = 2022; i < 2025; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Chosen_Semester->addItem(semester);
            }
        }
        comboBox_Chosen_Semester->addItem("(全部)");
        comboBox_Chosen_Semester->setCurrentText("(全部)");
        fillTableWidget_Chosen();
    }

    void switchStackedWidgetToStudent_Grade() {
        pushButton_1->setChecked(false);
        pushButton_2->setChecked(false);
        pushButton_3->setChecked(true);
        resizeTableWidget();
        stackedWidget->setCurrentIndex(3);
        updateChosenLessons();

        for (int i = 2022; i < 2025; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Grade_Semester->addItem(semester);
            }
        }
        comboBox_Grade_Semester->addItem("(全部)");
        comboBox_Grade_Semester->setCurrentText("(全部)");
        fillTableWidget_Grade();
    }

    void updateChosenLessons() {
        chosenLessons.clear();
        for (auto &&i: currentStudent.ChosenLessons) {
            Lesson lesson;
            getLessonByIdFromLocal(i, lesson);
            chosenLessons.append(lesson);
        }
    }

    void appendTextToTableItem(int row, int column, const QString &text) {
        QTableWidgetItem *item = tableWidget_Schedule->item(row, column);
        if (item) {
            QString existingText = item->text();
            if (!existingText.isEmpty()) {
                existingText += "----------\n";
            }
            existingText += text;
            item->setText(existingText);
        }
    }

    void getLessonById(const QString &Id, Lesson &lesson) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/getLessonInformation/" + Id;
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送GET请求
        QNetworkReply *reply = manager.get(request);

        // 创建一个事件循环，直到收到回复为止
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(3000);  // 3秒超时
        loop.exec();

        // 检查错误
        if (timer.isActive()) {
            // 请求在3秒内完成
            timer.stop();
            if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ContentNotFoundError) {
                QMessageBox::warning((QWidget *) this, "警告", "请求失败：" + QVariant::fromValue(reply->error()).toString());
            }
        } else {
            // 请求在3秒内未完成
            disconnect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            reply->abort();
            reply->deleteLater();
            QMessageBox::warning((QWidget *) this, "警告", "请求超时");
        }

        // 解析回复
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject json = doc.object();

        if (json["success"].toBool()) {
            // 使用回复中的数据填充课程对象
            lesson.Id = json["Id"].toString();
            lesson.LessonArea = json["LessonArea"].toString();
            lesson.LessonCredits = json["LessonCredits"].toInt();
            lesson.LessonSemester = json["LessonSemester"].toString();
            lesson.LessonName = json["LessonName"].toString();
            lesson.TeacherId = json["TeacherId"].toString();

            // 将选课学生的JSON数组转换为QVector
            QJsonArray lessonStudentsArray = json["LessonStudents"].toArray();
            for (auto &&i: lessonStudentsArray) {
                lesson.LessonStudents.append(i.toString());
            }

            // 将课程时间和地点的JSON对象转换为QMap
            QJsonObject lessonTimeAndLocationsObject = json["LessonTimeAndLocations"].toObject();
            for (auto &&i: lessonTimeAndLocationsObject.keys()) {
                QJsonArray timeAndLocationArray = lessonTimeAndLocationsObject[i].toArray();
                QVector<QString> timeAndLocation;
                for (auto &&j: timeAndLocationArray) {
                    timeAndLocation.append(j.toString());
                }
                lesson.LessonTimeAndLocations.insert(i, timeAndLocation);
            }
        } else {
            // 请求失败，获取并显示错误消息
            QString message = json["message"].toString();
            QMessageBox::warning(this, "警告", "获取课程信息失败：" + message);
        }
    }

    void getTeacherById(const QString &id, Teacher &teacher) const {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/getTeacherInformation/" + id;
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送GET请求
        QNetworkReply *reply = manager.get(request);

        // 创建一个事件循环，直到收到回复为止
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(3000);  // 3秒超时
        loop.exec();

        // 检查错误
        if (timer.isActive()) {
            // 请求在3秒内完成
            timer.stop();
            if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ContentNotFoundError) {
                QMessageBox::warning((QWidget *) this, "警告", "请求失败：" + QVariant::fromValue(reply->error()).toString());
            }
        } else {
            // 请求在3秒内未完成
            disconnect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            reply->abort();
            reply->deleteLater();
            QMessageBox::warning((QWidget *) this, "警告", "请求超时");
        }

        // 解析回复
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject json = doc.object();

        if (json["success"].toBool()) {
            // 使用回复中的数据填充教师对象
            teacher.Id = json["Id"].toString();
            teacher.Name = json["Name"].toString();
            teacher.Uint = json["Uint"].toString();

            // 将教授课程的JSON数组转换为QVector
            QJsonArray teachingLessonsArray = json["TeachingLessons"].toArray();
            for (auto &&i: teachingLessonsArray) {
                teacher.TeachingLessons.append(i.toString());
            }
        } else {
            // 请求失败，获取并显示错误消息
            QString message = json["message"].toString();
            QMessageBox::warning((QWidget *) this, "警告", "获取教师信息失败：" + message);
        }
    }

    void getTeacherByIdFromLocal(const QString &id, Teacher &teacher) {
        if (localTeachersTemp.contains(id)) {
            teacher = localTeachersTemp[id];
        } else {
            getTeacherById(id, teacher);
            localTeachersTemp.insert(id, teacher);
        }
    }

    void getLessonByIdFromLocal(const QString &id, Lesson &lesson) {
        if (localLessonsTemp.contains(id)) {
            lesson = localLessonsTemp[id];
        } else {
            getLessonById(id, lesson);
            localLessonsTemp.insert(id, lesson);
        }
    }

    void updateWelcomeWidget(const QString &account, int accountType) {
        if (accountType == STUDENT) {
            getStudentById(account, currentStudent);
            label_NameId->setText(currentStudent.Name + " " + currentStudent.Id);
            label_Unit->setText(currentStudent.College);
            label_Class->setText(currentStudent.Class);
        }
    }

    void getStudentById(const QString &id, Student &student) const {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/getStudentInformation/" + id;
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送GET请求
        QNetworkReply *reply = manager.get(request);

        // 创建一个事件循环，直到收到回复为止
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(3000);  // 3秒超时
        loop.exec();

        // 检查错误
        if (timer.isActive()) {
            // 请求在3秒内完成
            timer.stop();
            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::warning((QWidget *) this, "警告", "请求失败：" + QVariant::fromValue(reply->error()).toString());
            }
        } else {
            // 请求在3秒内未完成
            disconnect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            reply->abort();
            reply->deleteLater();
            QMessageBox::warning((QWidget *) this, "警告", "请求超时");
        }

        // 解析回复
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject json = doc.object();

        // 使用回复中的数据填充学生对象
        student.Id = json["Id"].toString();
        student.Name = json["Name"].toString();
        student.Sex = json["Sex"].toString();
        student.College = json["College"].toString();
        student.Major = json["Major"].toString();
        student.Class = json["Class"].toString();
        student.Age = json["Age"].toInt();
        student.PhoneNumber = json["PhoneNumber"].toString();
        student.DormitoryArea = json["DormitoryArea"].toString();
        student.DormitoryNum = json["DormitoryNum"].toString();

        // 将已选课程的JSON数组转换为QVector
        QJsonArray chosenLessonsArray = json["ChosenLessons"].toArray();
        for (auto &&i: chosenLessonsArray) {
            student.ChosenLessons.append(i.toString());
        }
    }

    void resizeTableWidget() {
        tableWidget_Schedule->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Chosen->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Grade->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());

        for (int i = 0; i < 7; i++) {
            int width = (tableWidget_Schedule->size().width() - 70) * 1 / 7;
            tableWidget_Schedule->setColumnWidth(i, width);
        }

        for (int i = 0; i < 9; i++) {
            int width = (tableWidget_Grade->size().width()) * 1 / 8;
            tableWidget_Grade->setColumnWidth(i, width);
        }

        for (int i = 0; i < 6; i++) {
            int width = (tableWidget_Chosen->size().width()) * 1 / 6;
            tableWidget_Chosen->setColumnWidth(i, width);
        }
        tableWidget_Schedule->resizeRowsToContents();
        tableWidget_Chosen->resizeRowsToContents();
    }

    void getStudentLessonGrade(const QString &studentId, const QString &lessonId, Grade &grade) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/getStudentLessonGrade/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 创建JSON对象
        QJsonObject json;
        json.insert("StudentId", studentId);
        json.insert("LessonId", lessonId);

        // 发送POST请求
        QJsonDocument doc(json);
        QByteArray data = doc.toJson();
        QNetworkReply *reply = manager.post(request, data);

        // 创建一个事件循环，直到收到回复为止
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(3000);  // 3秒超时
        loop.exec();

        // 检查错误
        if (timer.isActive()) {
            // 请求在3秒内完成
            timer.stop();
            if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ContentNotFoundError) {
                QMessageBox::warning((QWidget *) this, "警告", "请求失败：" + QVariant::fromValue(reply->error()).toString());
            }
        } else {
            // 请求在3秒内未完成
            disconnect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            reply->abort();
            reply->deleteLater();
            QMessageBox::warning((QWidget *) this, "警告", "请求超时");
        }

        // 解析回复
        QByteArray responseData = reply->readAll();
        doc = QJsonDocument::fromJson(responseData);
        json = doc.object();

        if (json["success"].toBool()) {
            // 使用回复中的数据填充成绩对象
            grade.StudentId = json["StudentId"].toString();
            grade.LessonId = json["LessonId"].toString();
            grade.ExamGrade = json["ExamGrade"].toDouble();
            grade.RegularGrade = json["RegularGrade"].toDouble();
            grade.TotalGrade = json["TotalGrade"].toDouble();
            grade.Retake = json["Retake"].toInt();

            // 将重修学期的JSON数组转换为QVector
            QJsonArray retakeSemestersArray = json["RetakeSemesters"].toArray();
            for (auto &&i: retakeSemestersArray) {
                grade.RetakeSemesters.append(i.toString());
            }

            // 将重修课程编号的JSON数组转换为QVector
            QJsonArray retakeLessonIdArray = json["RetakeLessonId"].toArray();
            for (auto &&i: retakeLessonIdArray) {
                QJsonObject retakeLessonIdObject = i.toObject();
                grade.RetakeLessonId.append(
                        {retakeLessonIdObject["LessonId"].toString(), retakeLessonIdObject["Semester"].toString()});
            }
        } else {
            // 请求失败，获取并显示错误消息
            QString message = json["message"].toString();
            QMessageBox::warning((QWidget *) this, "警告", "获取成绩信息失败：" + message);
        }
    }

    void getStudentLessonGradeFromLocal(const QString &studentId, const QString &lessonId, Grade &grade) {
        if (localGradesTemp.contains({studentId, lessonId})) {
            grade = localGradesTemp[{studentId, lessonId}];
        } else {
            getStudentLessonGrade(studentId, lessonId, grade);
            localGradesTemp.insert({studentId, lessonId}, grade);
        }
    }


public slots:

    void doLogout() {
        isLogin = false;
        close();
        loginForm.show();
    }

    void onLoginSuccess(const QString &jwt) {
        isLogin = true;
        serverURL = loginForm.lineEdit_ServerURL->text();
        JWT = jwt;
        Auth auth;
        decodeJWT(JWT, auth);
        updateButtons(auth.AccountType, auth.IsSuper);
        updateWelcomeWidget(auth.Account, auth.AccountType);
        stackedWidget->setCurrentIndex(0);
        show();
    }

    void fillTableWidget_Schedule() {
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 7; j++) {
                auto *item = new QTableWidgetItem();
                item->setTextAlignment(Qt::AlignCenter);
                tableWidget_Schedule->setItem(i, j, item);
            }
        }

        for (auto &&lesson: chosenLessons) {
            //检查是否为当前选择学期的课程
            if (lesson.LessonSemester != comboBox_Schedule_Semester->currentText()) {
                //qDebug() << "lesson.LessonSemester" << lesson.LessonSemester << "currentSemester" << currentSemester;
                continue;
            }
            QString lessonString;
            lessonString += lesson.LessonName + "\n";
            Teacher teacher;
            getTeacherByIdFromLocal(lesson.TeacherId, teacher);
            lessonString += teacher.Name + "\n";
            for (const auto &pair: lesson.LessonTimeAndLocations.toStdMap()) {
                QString lessonStringForOneTimeAndLocation = lessonString;
                const QString &weeks = pair.first;
                const QVector<QString> &details = pair.second;
                QString dayOfWeek = details[0].left(1);
                QString classPeriods = details[0].mid(1);
                QString location = details[1];

                lessonStringForOneTimeAndLocation += classPeriods + "(" + weeks + ")\n" + location + "\n";

                int column = dayOfWeek.toInt() - 1;
                //将 classPeriods最后的"节"去掉
                classPeriods.chop(1);
                //按每两位分割
                QStringList classPeriodsList;
                QVector<int> isAdded;
                isAdded.fill(0, 6);
                for (int i = 0; i < classPeriods.length(); i += 2) {
                    classPeriodsList.append(classPeriods.mid(i, 2));
                }
                for (auto &&classPeriod: classPeriodsList) {
                    //按表映射
                    //1-2节 -> 0, 3-5节 -> 1, 6-7节 -> 2, 8-9节 -> 3, 10-12节 -> 4, 13-15节 -> 5
                    if ((classPeriod == "01" || classPeriod == "02") && isAdded[0] == 0) {
                        appendTextToTableItem(0, column, lessonStringForOneTimeAndLocation);
                        isAdded[0] = 1;
                    } else if ((classPeriod == "03" || classPeriod == "04" || classPeriod == "05") && isAdded[1] == 0) {
                        appendTextToTableItem(1, column, lessonStringForOneTimeAndLocation);
                        isAdded[1] = 1;
                    } else if ((classPeriod == "06" || classPeriod == "07") && isAdded[2] == 0) {
                        appendTextToTableItem(2, column, lessonStringForOneTimeAndLocation);
                        isAdded[2] = 1;
                    } else if ((classPeriod == "08" || classPeriod == "09") && isAdded[3] == 0) {
                        appendTextToTableItem(3, column, lessonStringForOneTimeAndLocation);
                        isAdded[3] = 1;
                    } else if ((classPeriod == "10" || classPeriod == "11" || classPeriod == "12") && isAdded[4] == 0) {
                        appendTextToTableItem(4, column, lessonStringForOneTimeAndLocation);
                        isAdded[4] = 1;
                    } else if ((classPeriod == "13" || classPeriod == "14" || classPeriod == "15") && isAdded[5] == 0) {
                        appendTextToTableItem(5, column, lessonStringForOneTimeAndLocation);
                        isAdded[5] = 1;
                    }
                }
            }
        }
        tableWidget_Schedule->resizeRowsToContents();
    }

    void fillTableWidget_Chosen() {
        // 清空表格
        tableWidget_Chosen->setRowCount(0);
        QVector<Lesson> ToBeShownLessons;
        for (auto &&lesson: chosenLessons) {
            //检查是否为当前选择学期的课程
            if (comboBox_Chosen_Semester->currentText() == "(全部)" ||
                lesson.LessonSemester == comboBox_Chosen_Semester->currentText()) {
                // 检查搜索框是否为空
                if (lineEdit_Chosen_Search->text().isEmpty()) {
                    ToBeShownLessons.append(lesson);
                } else {
                    //空格分词
                    QStringList words = lineEdit_Chosen_Search->text().split(" ");
                    //在课程编号、课程名称、授课教师、上课校区中匹配，全部满足则加入
                    bool isMatched = true;
                    for (auto &&word: words) {
                        Teacher teacher;
                        getTeacherByIdFromLocal(lesson.TeacherId, teacher);
                        if (lesson.Id.contains(word) || lesson.LessonName.contains(word) ||
                            lesson.LessonArea.contains(word) || teacher.Name.contains(word)) {
                            continue;
                        } else {
                            isMatched = false;
                            break;
                        }
                    }
                    if (isMatched) {
                        ToBeShownLessons.append(lesson);
                    }
                }
            }
        }

        for (int i = 0; i < ToBeShownLessons.size(); i++) {
            // 插入新行
            tableWidget_Chosen->insertRow(i);

            // 获取课程信息
            Lesson lesson = ToBeShownLessons[i];
            Teacher teacher;
            getTeacherByIdFromLocal(lesson.TeacherId, teacher);

            // 创建表格项并设置文本居中
            auto *item0 = new QTableWidgetItem(lesson.LessonSemester);
            item0->setTextAlignment(Qt::AlignCenter);
            auto *item1 = new QTableWidgetItem(lesson.Id);
            item1->setTextAlignment(Qt::AlignCenter);
            auto *item2 = new QTableWidgetItem(lesson.LessonName);
            item2->setTextAlignment(Qt::AlignCenter);
            auto *item3 = new QTableWidgetItem(teacher.Name);
            item3->setTextAlignment(Qt::AlignCenter);
            auto *item4 = new QTableWidgetItem(lesson.LessonArea);
            item4->setTextAlignment(Qt::AlignCenter);
            QString timeAndLocation;
            for (auto &&pair: lesson.LessonTimeAndLocations.toStdMap()) {
                timeAndLocation += pair.first + " " + pair.second[0] + " " + pair.second[1] + ",";
            }
            timeAndLocation.chop(1);
            auto *item5 = new QTableWidgetItem(timeAndLocation);
            item5->setTextAlignment(Qt::AlignCenter);

            // 将表格项添加到表格中
            tableWidget_Chosen->setItem(i, 0, item0);
            tableWidget_Chosen->setItem(i, 1, item1);
            tableWidget_Chosen->setItem(i, 2, item2);
            tableWidget_Chosen->setItem(i, 3, item3);
            tableWidget_Chosen->setItem(i, 4, item4);
            tableWidget_Chosen->setItem(i, 5, item5);
        }

        // 调整行的大小以适应内容
        tableWidget_Chosen->resizeRowsToContents();
    }

    void clearCache() {
        localTeachersTemp.clear();
        localLessonsTemp.clear();
        localGradesTemp.clear();
        QMessageBox::information(this, "提示", "缓存已清除");
    }

    void fillTableWidget_Grade() {
        tableWidget_Grade->setRowCount(0);
        int CreditSum = 0;
        QVector<Lesson> ToBeShownLessons;
        for (auto &&lesson: chosenLessons) {
            Grade grade;
            getStudentLessonGradeFromLocal(currentStudent.Id, lesson.Id, grade);
            if (grade.TotalGrade >= 60) {
                CreditSum += lesson.LessonCredits;
            }
            //检查是否为当前选择学期的课程
            if (comboBox_Grade_Semester->currentText() == "(全部)" ||
                lesson.LessonSemester == comboBox_Grade_Semester->currentText()) {
                // 检查搜索框是否为空
                if (lineEdit_Grade_Search->text().isEmpty()) {
                    ToBeShownLessons.append(lesson);
                } else {
                    //空格分词
                    QStringList words = lineEdit_Grade_Search->text().split(" ");
                    //在课程编号、课程名称、授课教师、上课校区中匹配，全部满足则加入
                    bool isMatched = true;
                    for (auto &&word: words) {
                        if (lesson.Id.contains(word) || lesson.LessonName.contains(word)) {
                            continue;
                        } else {
                            isMatched = false;
                            break;
                        }
                    }
                    if (isMatched) {
                        ToBeShownLessons.append(lesson);
                    }
                }
            }
        }
        label_Grade_CreditSum->setText(QString::number(CreditSum));
        QMap<QString, int> RetakeLessons;
        // 从chosenLessons中获取所有课程的成绩
        for (auto &&lesson: ToBeShownLessons) {
            Grade grade;
            getStudentLessonGradeFromLocal(currentStudent.Id, lesson.Id, grade);
            // 插入新行
            int row = tableWidget_Grade->rowCount();
            tableWidget_Grade->insertRow(row);

            // 创建表格项并设置文本居中
            auto *item0 = new QTableWidgetItem(lesson.LessonSemester);
            item0->setTextAlignment(Qt::AlignCenter);
            auto *item1 = new QTableWidgetItem(lesson.Id);
            item1->setTextAlignment(Qt::AlignCenter);
            auto *item2 = new QTableWidgetItem(lesson.LessonName);
            item2->setTextAlignment(Qt::AlignCenter);
            auto *item3 = new QTableWidgetItem(QString::number(grade.TotalGrade));
            item3->setTextAlignment(Qt::AlignCenter);
            auto *item4 = new QTableWidgetItem(QString::number(grade.RegularGrade));
            item4->setTextAlignment(Qt::AlignCenter);
            auto *item5 = new QTableWidgetItem(QString::number(grade.ExamGrade));
            item5->setTextAlignment(Qt::AlignCenter);
            auto *item6 = new QTableWidgetItem(QString::number(lesson.LessonCredits));
            item6->setTextAlignment(Qt::AlignCenter);
            for (int i = 0; i < grade.RetakeLessonId.size(); i++) {
                RetakeLessons.insert(grade.RetakeLessonId[i], i + 1);
            }
            QString Type;
            if (grade.Retake == 2) {
                Type = "重修" + QString::number(RetakeLessons[lesson.Id]);
            } else {
                Type = "正常考试";
            }
            auto *item7 = new QTableWidgetItem(Type);
            item7->setTextAlignment(Qt::AlignCenter);

            // 将表格项添加到表格中
            tableWidget_Grade->setItem(row, 0, item0);
            tableWidget_Grade->setItem(row, 1, item1);
            tableWidget_Grade->setItem(row, 2, item2);
            tableWidget_Grade->setItem(row, 3, item3);
            tableWidget_Grade->setItem(row, 4, item4);
            tableWidget_Grade->setItem(row, 5, item5);
            tableWidget_Grade->setItem(row, 6, item6);
            tableWidget_Grade->setItem(row, 7, item7);

            tableWidget_Grade->resizeRowsToContents();
        }
    }

signals:
protected:
    void resizeEvent(QResizeEvent *event) override {
        QMainWindow::resizeEvent(event);  // 调用父类的resizeEvent函数
        resizeTableWidget();
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    AIMSMainWindow mainWindow;
    mainWindow.doLogin();

    return QApplication::exec();
}

#include "main.moc"