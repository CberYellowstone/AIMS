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
#include "ui_StudentList.h"
#include "ui_ChangePasswordForm.h"
#include "../server/database.h"

#define SALT "AIMS"

class ChangePasswordForm : public QMainWindow, public Ui::ChangePasswordForm {
Q_OBJECT

public:
    QString newPassword{};

    explicit ChangePasswordForm(QMainWindow *parent = nullptr) : QMainWindow(parent) {
        setupUi(this);
    }

public slots:

    void changePassword();

};

class StudentListForm : public QMainWindow, public Ui::StudentListForm {
Q_OBJECT

public:
    QString currentLessonId;
    QString lastClickedStudentId{};
    QPair<int, int> lastClickedCell{};
    QStringList lessonStudentList{};
    QStringList lessonStudentToAssignList{};
    QStringList lessonStudentToUnassignList{};

    explicit StudentListForm(QMainWindow *parent = nullptr) : QMainWindow(parent) {
        setupUi(this);
    }

public slots:

    void showLessonStudentList(const QString &lessonId) {
        currentLessonId = lessonId;
        setWindowTitle("课程学生列表");
        stackedWidget->setCurrentIndex(0);
        fillTableWidget_LessonStudent();
        lastClickedStudentId = "";
        show();
    }

    void showLessonGradeList(const QString &lessonId) {
        currentLessonId = lessonId;
        setWindowTitle("课程成绩列表");
        stackedWidget->setCurrentIndex(1);
        disconnect(tableWidget_LessonGrade, &QTableWidget::itemChanged, this, &StudentListForm::doCheckAndSendGrade);
        fillTableWidget_LessonGrade();

        show();
    }

    void showLessonChosenList(const QString &lessonId) {
        currentLessonId = lessonId;
        setWindowTitle("课程已选学生列表");
        stackedWidget->setCurrentIndex(2);
        fillTableWidget_LessonChosen();
        show();
    }

    void fillTableWidget_LessonStudent();

    void fillTableWidget_LessonGrade();

    void fillTableWidget_LessonChosen();

    void onTableWidgetLessonGradeCellDoubleClicked(int row, int column) {
        lastClickedCell = {row, column};
        lastClickedStudentId = tableWidget_LessonGrade->item(row, 0)->text();
    }

    void doCheckAndSendGrade();

    void finishChoosing();

    void addStudentToLesson() {
        tableWidget_LessonChosenStudent->insertRow(tableWidget_LessonChosenStudent->rowCount());
        auto *item = new QTableWidgetItem;
        item->setTextAlignment(Qt::AlignCenter);
        tableWidget_LessonChosenStudent->setItem(tableWidget_LessonChosenStudent->rowCount() - 1, 0, item);
    }

    void removeStudentFromLesson() {
        tableWidget_LessonChosenStudent->removeRow(tableWidget_LessonChosenStudent->currentRow());
    }
};

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
//        lineEdit_Account->setText("114514");
//        lineEdit_Password->setText("1");

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
    Teacher currentTeacher;
    QVector<Lesson> chosenLessons;
    QVector<Lesson> teachingLessons;
    QMap<QString, Student> localStudentsTemp;
    QMap<QString, Teacher> localTeachersTemp;
    QMap<QString, Lesson> localLessonsTemp;
    QVector<Lesson> localLessonsListTemp;
    QVector<Student> localStudentsListTemp;
    QVector<Teacher> localTeachersListTemp;
    QString currentSemester;
    int currentYear{};
    QMap<QPair<QString, QString>, Grade> localGradesTemp;
    QMap<QString, QVector<QString>> localLessonClassesTemp;

    explicit AIMSMainWindow(QMainWindow *parent = nullptr) : QMainWindow(parent) {
        setupUi(this);
        calculateCurrentSemester();
        connect(&loginForm, &LoginForm::LoginSuccess, this, &AIMSMainWindow::onLoginSuccess);
    }

    void calculateCurrentSemester() {
        //学期组成: 起始年份-结束年份-学期
        //学期和月份的对应关系:9-2->1, 3-8->2

        QDateTime currentDateTime = QDateTime::currentDateTime();
        currentYear = currentDateTime.date().year();
        int currentMonth = currentDateTime.date().month();
        //计算当前学期
        if (currentMonth >= 9 && currentMonth <= 12) {
            currentSemester = QString::number(currentYear) + "-" + QString::number(currentYear + 1) + "-1";
        } else if (currentMonth >= 3 && currentMonth <= 8) {
            currentSemester = QString::number(currentYear - 1) + "-" + QString::number(currentYear) + "-2";
        } else if (currentMonth >= 1 && currentMonth <= 2) {
            currentSemester = QString::number(currentYear - 1) + "-" + QString::number(currentYear) + "-1";
        }
        // qDebug() << "currentSemester" << currentSemester;
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
        } else if (AccountType == TEACHER) {
            pushButton_1->setText("我的排课");
            pushButton_2->setText("我的课程");
            pushButton_3->setText("学生成绩");
            connect(pushButton_1, &QPushButton::clicked, this, &AIMSMainWindow::switchStackedWidgetToTeacher_Schedule);
            connect(pushButton_2, &QPushButton::clicked, this, &AIMSMainWindow::switchStackedWidgetToTeacher_Teaching);
            connect(pushButton_3, &QPushButton::clicked, this, &AIMSMainWindow::switchStackedWidgetToTeacher_Grade);
            if (IsSuper) {
                pushButton_4->setText("用户管理");
                pushButton_5->setText("课程管理");
                pushButton_6->setText("成绩管理");
                connect(pushButton_4, &QPushButton::clicked, this, &AIMSMainWindow::switchStackedWidgetToSuper_Account);
                connect(pushButton_5, &QPushButton::clicked, this, &AIMSMainWindow::switchStackedWidgetToSuper_Lesson);
                connect(pushButton_6, &QPushButton::clicked, this, &AIMSMainWindow::switchStackedWidgetToSuper_Grade);
            } else {
                pushButton_4->hide();
                pushButton_5->hide();
                pushButton_6->hide();
            }
        } else {
            QMessageBox::warning(this, "警告", "未知用户类型");
        }
    }

    void switchStackedWidgetToStudent_Schedule() {
        pushButton_1->setChecked(true);
        pushButton_2->setChecked(false);
        pushButton_3->setChecked(false);
        resizeWidget();
        stackedWidget->setCurrentIndex(1);
        updateChosenLessons();

        for (int i = currentYear - 2; i < currentYear + 1; i++) {
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
        resizeWidget();
        stackedWidget->setCurrentIndex(2);
        updateChosenLessons();

        for (int i = currentYear - 2; i < currentYear + 1; i++) {
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
        resizeWidget();
        stackedWidget->setCurrentIndex(3);
        updateChosenLessons();

        for (int i = currentYear - 2; i < currentYear + 1; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Grade_Semester->addItem(semester);
            }
        }
        comboBox_Grade_Semester->addItem("(全部)");
        comboBox_Grade_Semester->setCurrentText("(全部)");
        fillTableWidget_Grade();
    }

    void switchStackedWidgetToTeacher_Schedule() {
        pushButton_1->setChecked(true);
        pushButton_2->setChecked(false);
        pushButton_3->setChecked(false);
        pushButton_4->setChecked(false);
        pushButton_5->setChecked(false);
        pushButton_6->setChecked(false);
        resizeWidget();
        stackedWidget->setCurrentIndex(4);
        updateTeachingLessons();

        for (int i = currentYear - 2; i < currentYear + 1; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Teacher_Schedule_Semester->addItem(semester);
            }
        }
        comboBox_Teacher_Schedule_Semester->setCurrentText(currentSemester);
        fillTableWidget_Teacher_Schedule();
    }

    void switchStackedWidgetToTeacher_Teaching() {
        pushButton_1->setChecked(false);
        pushButton_2->setChecked(true);
        pushButton_3->setChecked(false);
        pushButton_4->setChecked(false);
        pushButton_5->setChecked(false);
        pushButton_6->setChecked(false);
        resizeWidget();
        stackedWidget->setCurrentIndex(5);
        updateTeachingLessons();

        for (int i = currentYear - 2; i < currentYear + 1; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Teacher_Teaching_Semester->addItem(semester);
            }
        }
        comboBox_Teacher_Teaching_Semester->addItem("(全部)");
        comboBox_Teacher_Teaching_Semester->setCurrentText("(全部)");
        fillTableWidget_Teacher_Teaching();
    }

    void switchStackedWidgetToTeacher_Grade() {
        pushButton_1->setChecked(false);
        pushButton_2->setChecked(false);
        pushButton_3->setChecked(true);
        pushButton_4->setChecked(false);
        pushButton_5->setChecked(false);
        pushButton_6->setChecked(false);
        resizeWidget();
        stackedWidget->setCurrentIndex(6);
        updateTeachingLessons();

        for (int i = currentYear - 2; i < currentYear + 1; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Teacher_Grade_Semester->addItem(semester);
            }
        }
        comboBox_Teacher_Grade_Semester->addItem("(全部)");
        comboBox_Teacher_Grade_Semester->setCurrentText("(全部)");
        fillTableWidget_Teacher_Grade();
    }

    void switchStackedWidgetToSuper_Lesson() {
        pushButton_1->setChecked(false);
        pushButton_2->setChecked(false);
        pushButton_3->setChecked(false);
        pushButton_4->setChecked(false);
        pushButton_5->setChecked(true);
        pushButton_6->setChecked(false);
        resizeWidget();
        stackedWidget->setCurrentIndex(8);

        for (int i = currentYear - 2; i < currentYear + 1; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Super_Lesson_Semester->addItem(semester);
            }
        }
        comboBox_Super_Lesson_Semester->addItem("(全部)");
        comboBox_Super_Lesson_Semester->setCurrentText("(全部)");

        for (int i = currentYear - 2; i < currentYear + 1; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Super_Lesson_Assign_Semester->addItem(semester);
            }
        }
        comboBox_Super_Lesson_Assign_Semester->addItem("(全部)");
        comboBox_Super_Lesson_Assign_Semester->setCurrentText("(全部)");
        fillTableWidget_Super_Lesson();
        fillTableWidget_Super_Lesson_Assign();
    }

    void updateStudentLessonGrade(const Grade &grade) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/updateStudentLessonGrade/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());


        // 发送POST请求
        QJsonObject json;
        json.insert("LessonId", grade.LessonId);
        json.insert("StudentId", grade.StudentId);
        json.insert("ExamGrade", grade.ExamGrade);
        json.insert("RegularGrade", grade.RegularGrade);
        json.insert("TotalGrade", grade.TotalGrade);

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
    }

    void switchStackedWidgetToSuper_Grade() {
        pushButton_1->setChecked(false);
        pushButton_2->setChecked(false);
        pushButton_3->setChecked(false);
        pushButton_4->setChecked(false);
        pushButton_5->setChecked(false);
        pushButton_6->setChecked(true);
        resizeWidget();
        stackedWidget->setCurrentIndex(9);

        for (int i = currentYear - 2; i < currentYear + 1; i++) {
            for (int j = 1; j < 3; j++) {
                QString semester = QString::number(i) + "-" + QString::number(i + 1) + "-" + QString::number(j);
                comboBox_Super_Grade_Semester->addItem(semester);
            }
        }
        comboBox_Super_Grade_Semester->addItem("(全部)");
        comboBox_Super_Grade_Semester->setCurrentText("(全部)");
        fillTableWidget_Super_Grade();
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

    void switchStackedWidgetToSuper_Account() {
        pushButton_1->setChecked(false);
        pushButton_2->setChecked(false);
        pushButton_3->setChecked(false);
        pushButton_4->setChecked(true);
        pushButton_5->setChecked(false);
        pushButton_6->setChecked(false);
        resizeWidget();
        stackedWidget->setCurrentIndex(7);
        fillTableWidget_Super_Student();
        fillTableWidget_Super_Teacher();
    }

    void appendTextToTeacherTableItem(int row, int column, const QString &text) {
        QTableWidgetItem *item = tableWidget_Teacher_Schedule->item(row, column);
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
            teacher.Unit = json["Unit"].toString();

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

    void updateAccount(const Auth &auth) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/updateAccount/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("Account", auth.Account);
        json.insert("AccountType", auth.AccountType);
        json.insert("IsSuper", auth.IsSuper);

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
    }

    void updateStudentInformation(const Student &student) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/updateStudentInformation/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("Id", student.Id);
        json.insert("Name", student.Name);
        json.insert("College", student.College);
        json.insert("Class", student.Class);
        json.insert("Age", student.Age);
        json.insert("Major", student.Major);
        json.insert("PhoneNumber", student.PhoneNumber);
        json.insert("DormitoryArea", student.DormitoryArea);
        json.insert("DormitoryNum", student.DormitoryNum);
        json.insert("Sex", student.Sex);

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
    }

    void updateTeacherInformation(const Teacher &teacher) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/updateTeacherInformation/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("Id", teacher.Id);
        json.insert("Name", teacher.Name);
        json.insert("Unit", teacher.Unit);

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
    }

    void deleteTeacher(const QString &teacherId) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/deleteTeacher/";
        request.setUrl(QUrl(URL));

        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("Id", teacherId);

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
    }

    void deleteStudent(const QString &studentId) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/deleteStudent/";
        request.setUrl(QUrl(URL));

        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("Id", studentId);

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
    }

    void getLessonByIdFromLocal(const QString &id, Lesson &lesson) {
        if (localLessonsTemp.contains(id)) {
            lesson = localLessonsTemp[id];
        } else {
            getLessonById(id, lesson);
            localLessonsTemp.insert(id, lesson);
        }
    }

    void getStudentByIdFromLocal(const QString &id, Student &student) {
        if (localStudentsTemp.contains(id)) {
            student = localStudentsTemp[id];
        } else {
            getStudentById(id, student);
            localStudentsTemp.insert(id, student);
        }
    }

    void addAccount(const Auth &auth, const QString &password) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/addAccount/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        QString Secret = QCryptographicHash::hash(
                QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Md5).toHex() + SALT,
                QCryptographicHash::Md5).toHex();
        // 发送POST请求
        QJsonObject json;
        json.insert("Account", auth.Account);
        json.insert("AccountType", auth.AccountType);
        json.insert("IsSuper", auth.IsSuper);
        json.insert("Secret", Secret);

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
    }

    void updateTeachingLessons() {
        teachingLessons.clear();
        for (auto &&i: currentTeacher.TeachingLessons) {
            Lesson lesson;
            getLessonByIdFromLocal(i, lesson);
            teachingLessons.append(lesson);
        }
    }

    void updateWelcomeWidget(const QString &account, int accountType) {
        if (accountType == STUDENT) {
            getStudentByIdFromLocal(account, currentStudent);
            label_NameId->setText("学生 " + currentStudent.Name + " " + currentStudent.Id);
            label_Unit->setText(currentStudent.College);
            label_ClassTitle->show();
            label_Class->show();
            label_Class->setText(currentStudent.Class);
        } else {
            getTeacherByIdFromLocal(account, currentTeacher);
            label_NameId->setText("教师 " + currentTeacher.Name + " " + currentTeacher.Id);
            label_Unit->setText(currentTeacher.Unit);
            label_ClassTitle->hide();
            label_ClassTitle->hide();
        }
    }

    void updateLessonInformation(const Lesson &lesson) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/updateLessonInformation/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("Id", lesson.Id);
        json.insert("LessonName", lesson.LessonName);
        json.insert("LessonArea", lesson.LessonArea);
        json.insert("LessonCredits", lesson.LessonCredits);
        json.insert("LessonSemester", lesson.LessonSemester);
        json.insert("TeacherId", lesson.TeacherId);

        // 将选课学生的QVector转换为JSON数组
        QJsonArray lessonStudentsArray;
        for (auto &&i: lesson.LessonStudents) {
            lessonStudentsArray.append(i);
        }
        json.insert("LessonStudents", lessonStudentsArray);

        // 将课程时间和地点的QMap转换为JSON对象
        QJsonObject lessonTimeAndLocationsObject;
        for (auto &&i: lesson.LessonTimeAndLocations.keys()) {
            QJsonArray timeAndLocationArray;
            for (auto &&j: lesson.LessonTimeAndLocations[i]) {
                timeAndLocationArray.append(j);
            }
            lessonTimeAndLocationsObject.insert(i, timeAndLocationArray);
        }
        json.insert("LessonTimeAndLocations", lessonTimeAndLocationsObject);

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
    }

    void updateLessonChosenStudent(const Lesson &lesson) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/updateLessonChosenStudent/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("Id", lesson.Id);

        // 将选课学生的QVector转换为JSON数组
        QJsonArray lessonStudentsArray;
        for (auto &&i: lesson.LessonStudents) {
            lessonStudentsArray.append(i);
        }
        json.insert("LessonStudents", lessonStudentsArray);

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
    }

    void addChosenLesson(const QString &studentId, const QString &lessonId) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/addChosenLesson/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("studentId", studentId);
        json.insert("lessonId", lessonId);

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
    }

    void addRetake(const QString &studentId, const QString &needRetakeLessonId, const QString &retakeLessonId) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/addRetake/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("studentId", studentId);
        json.insert("needRetakeLessonId", needRetakeLessonId);
        json.insert("lessonId", retakeLessonId);

        Lesson needRetakeLesson;
        getLessonByIdFromLocal(needRetakeLessonId, needRetakeLesson);
        json.insert("needRetakeLessonSemester", needRetakeLesson.LessonSemester);

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
    }

    void deleteChosenLesson(const QString &studentId, const QString &lessonId) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/deleteChosenLesson/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("studentId", studentId);
        json.insert("lessonId", lessonId);

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
    }

    void deleteLesson(const QString &lessonId) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/deleteLesson/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("Id", lessonId);

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
    }

    void changePassword(const QString &newPassword) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/changePassword/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        QString Secret = QCryptographicHash::hash(
                QCryptographicHash::hash(newPassword.toUtf8(), QCryptographicHash::Md5).toHex() + SALT,
                QCryptographicHash::Md5).toHex();
        // 判断当前是学生还是教师
        QString Account;
        if (currentStudent.Id.isEmpty()) {
            Account = currentTeacher.Id;
        } else {
            Account = currentStudent.Id;
        }
        // 发送POST请求
        QJsonObject json;
        json.insert("Account", Account);
        json.insert("Secret", Secret);

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
        doLogout();
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

    void resizeWidget() {
        tableWidget_Schedule->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Chosen->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Grade->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Teacher_Schedule->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Teacher_Teaching->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Teacher_Grade->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tabWidget_Super_Grade->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Super_Grade->setFixedSize(tabWidget_Super_Grade->size().width() - 5,
                                              tabWidget_Super_Grade->size().height() - 70);
        tabWidget_Super_Account->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Super_Student->setFixedSize(tabWidget_Super_Account->size().width() - 5,
                                                tabWidget_Super_Account->size().height() - 70);
        tableWidget_Super_Teacher->setFixedSize(tabWidget_Super_Account->size().width() - 5,
                                                tabWidget_Super_Account->size().height() - 70);
        tabWidget_Super_Lesson->setFixedSize(stackedWidget->size().width(), stackedWidget->size().height());
        tableWidget_Super_Lesson->setFixedSize(tabWidget_Super_Lesson->size().width() - 5,
                                               tabWidget_Super_Lesson->size().height() - 70);
        tableWidget_Super_Lesson_Assign->setFixedSize(tabWidget_Super_Lesson->size().width() - 5,
                                                      tabWidget_Super_Lesson->size().height() - 70);
        for (int i = 0; i < 7; i++) {
            int width = (tableWidget_Schedule->size().width() - 70) * 1 / 7;
            tableWidget_Schedule->setColumnWidth(i, width);
        }
        for (int i = 0; i < 7; i++) {
            int width = (tableWidget_Teacher_Schedule->size().width() - 70) * 1 / 7;
            tableWidget_Teacher_Schedule->setColumnWidth(i, width);
        }
        for (int i = 0; i < 9; i++) {
            int width = (tableWidget_Grade->size().width() - 20) * 1 / 8;
            tableWidget_Grade->setColumnWidth(i, width);
        }
        for (int i = 0; i < 6; i++) {
            int width = (tableWidget_Chosen->size().width() - 20) * 1 / 6;
            tableWidget_Chosen->setColumnWidth(i, width);
        }
        for (int i = 0; i < 8; i++) {
            int width = (tableWidget_Teacher_Teaching->size().width() - 20) * 1 / 8;
            tableWidget_Teacher_Teaching->setColumnWidth(i, width);
        }
        for (int i = 0; i < 8; i++) {
            int width = (tableWidget_Teacher_Grade->size().width() - 20) * 1 / 8;
            tableWidget_Teacher_Grade->setColumnWidth(i, width);
        }
        for (int i = 0; i < 8; i++) {
            int width = (tableWidget_Super_Grade->size().width() - 20) * 1 / 8;
            tableWidget_Super_Grade->setColumnWidth(i, width);
        }
        for (int i = 0; i < 11; i++) {
            int width = (tableWidget_Super_Student->size().width() - 20) * 1 / 11;
            tableWidget_Super_Student->setColumnWidth(i, width);
        }
        for (int i = 0; i < 5; i++) {
            int width = (tableWidget_Super_Teacher->size().width() - 20) * 1 / 5;
            tableWidget_Super_Teacher->setColumnWidth(i, width);
        }
        for (int i = 0; i < 8; i++) {
            int width = (tableWidget_Super_Lesson->size().width() - 20) * 1 / 8;
            tableWidget_Super_Lesson->setColumnWidth(i, width);
        }
        for (int i = 0; i < 8; i++) {
            int width = (tableWidget_Super_Lesson_Assign->size().width() - 20) * 1 / 8;
            tableWidget_Super_Lesson_Assign->setColumnWidth(i, width);
        }
        tableWidget_Teacher_Teaching->resizeRowsToContents();
        tableWidget_Schedule->resizeRowsToContents();
        tableWidget_Teacher_Schedule->resizeRowsToContents();
        tableWidget_Chosen->resizeRowsToContents();
        tableWidget_Grade->resizeRowsToContents();
        tableWidget_Teacher_Grade->resizeRowsToContents();
        tableWidget_Super_Grade->resizeRowsToContents();
        tableWidget_Super_Student->resizeRowsToContents();
        tableWidget_Super_Teacher->resizeRowsToContents();
        tableWidget_Super_Lesson->resizeRowsToContents();
    }

    bool checkIsSUPER(QString &Account) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/checkAccountSUPER/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QJsonObject json;
        json.insert("Account", Account);

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

        bool isSuper = false;
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

        return json["success"].toBool();
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

    void listLessonClasses(const QString &lessonId, QVector<QString> &classes) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/listLessonClasses/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 创建JSON对象
        QJsonObject json;
        json.insert("LessonId", lessonId);

        // 将JSON对象转换为字节数组
        QByteArray data = QJsonDocument(json).toJson();

        // 发送POST请求
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
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject docJson = doc.object();

        if (docJson["success"].toBool()) {
            // 使用回复中的数据填充班级对象
            QJsonArray classesArray = docJson["classes"].toArray();
            for (auto &&i: classesArray) {
                classes.append(i.toString());
            }
        } else {
            // 请求失败，获取并显示错误消息
            QString message = docJson["message"].toString();
            QMessageBox::warning((QWidget *) this, "警告", "获取班级信息失败：" + message);
        }
        classes.sort();
    }

    void listLessonClassesFromLocal(const QString &lessonId, QVector<QString> &classes) {
        if (localLessonClassesTemp.contains(lessonId)) {
            classes = localLessonClassesTemp[lessonId];
        } else {
            listLessonClasses(lessonId, classes);
            localLessonClassesTemp.insert(lessonId, classes);
        }
    }

    void listLessonsFromLocal(QVector<Lesson> &lessons) {
        if (localLessonsListTemp.isEmpty()) {
            listLessons(localLessonsListTemp);
        }
        for (auto &&i: localLessonsListTemp) {
            lessons.append(i);
        }
    }

    void listLessons(QVector<Lesson> &lessons) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/listLessons/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QNetworkReply *reply = manager.post(request, "");

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
        QJsonObject docJson = doc.object();

        if (docJson["success"].toBool()) {
            // 使用回复中的数据填充课程对象
            QJsonArray lessonsArray = docJson["lessons"].toArray();
            for (auto &&i: lessonsArray) {
                Lesson lesson;
                QJsonObject lessonObject = i.toObject();
                lesson.Id = lessonObject["Id"].toString();
                lesson.LessonArea = lessonObject["LessonArea"].toString();
                lesson.LessonCredits = lessonObject["LessonCredits"].toInt();
                lesson.LessonSemester = lessonObject["LessonSemester"].toString();
                lesson.LessonName = lessonObject["LessonName"].toString();
                lesson.TeacherId = lessonObject["TeacherId"].toString();

                // 将选课学生的JSON数组转换为QVector
                QJsonArray lessonStudentsArray = lessonObject["LessonStudents"].toArray();
                for (auto &&j: lessonStudentsArray) {
                    lesson.LessonStudents.append(j.toString());
                }

                // 将课程时间和地点的JSON对象转换为QMap
                QJsonObject lessonTimeAndLocationsObject = lessonObject["LessonTimeAndLocations"].toObject();
                for (auto &&j: lessonTimeAndLocationsObject.keys()) {
                    QJsonArray timeAndLocationArray = lessonTimeAndLocationsObject[j].toArray();
                    QVector<QString> timeAndLocation;
                    for (auto &&k: timeAndLocationArray) {
                        timeAndLocation.append(k.toString());
                    }
                    lesson.LessonTimeAndLocations.insert(j, timeAndLocation);
                }
                lessons.append(lesson);
            }
        } else {
            // 请求失败，获取并显示错误消息
            QString message = docJson["message"].toString();
            QMessageBox::warning((QWidget *) this, "警告", "获取课程信息失败：" + message);
        }
    }


    void listStudentsFromLocal(QVector<Student> &students) {
        if (localStudentsListTemp.isEmpty()) {
            listStudents(localStudentsListTemp);
        }
        for (auto &&i: localStudentsListTemp) {
            students.append(i);
        }
    }

    void listTeachersFromLocal(QVector<Teacher> &teachers) {
        if (localTeachersListTemp.isEmpty()) {
            listTeachers(localTeachersListTemp);
        }
        for (auto &&i: localTeachersListTemp) {
            teachers.append(i);
        }
    }

    void listTeachers(QVector<Teacher> &teachers) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/listTeachers/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QNetworkReply *reply = manager.post(request, "");

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
        QJsonObject docJson = doc.object();

        if (docJson["success"].toBool()) {
            // 使用回复中的数据填充教师对象
            QJsonArray teachersArray = docJson["teachers"].toArray();
            for (auto &&i: teachersArray) {
                Teacher teacher;
                QJsonObject teacherObject = i.toObject();
                teacher.Id = teacherObject["Id"].toString();
                teacher.Name = teacherObject["Name"].toString();
                teacher.Unit = teacherObject["Unit"].toString();

                // 将教授课填入QVector
                QJsonArray teachingLessonsArray = teacherObject["TeachingLessons"].toArray();
                for (auto &&j: teachingLessonsArray) {
                    teacher.TeachingLessons.append(j.toString());
                }
                teachers.append(teacher);
            }
        } else {
            // 请求失败，获取并显示错误消息
            QString message = docJson["message"].toString();
            QMessageBox::warning((QWidget *) this, "警告", "获取教师列表失败：" + message);
        }
    }

    void listStudents(QVector<Student> &students) {
        QNetworkAccessManager manager;
        QNetworkRequest request;

        // 设置请求的URL
        QString URL = serverURL + "/api/listStudents/";
        request.setUrl(QUrl(URL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + JWT).toUtf8());

        // 发送POST请求
        QNetworkReply *reply = manager.post(request, "");

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
        QJsonObject docJson = doc.object();

        if (docJson["success"].toBool()) {
            // 使用回复中的数据填充学生对象
            QJsonArray studentsArray = docJson["students"].toArray();
            for (auto &&i: studentsArray) {
                Student student;
                QJsonObject studentObject = i.toObject();
                student.Id = studentObject["Id"].toString();
                student.Name = studentObject["Name"].toString();
                student.Sex = studentObject["Sex"].toString();
                student.College = studentObject["College"].toString();
                student.Major = studentObject["Major"].toString();
                student.Class = studentObject["Class"].toString();
                student.Age = studentObject["Age"].toInt();
                student.PhoneNumber = studentObject["PhoneNumber"].toString();
                student.DormitoryArea = studentObject["DormitoryArea"].toString();
                student.DormitoryNum = studentObject["DormitoryNum"].toString();

                // 将已选课程的JSON数组转换为QVector
                QJsonArray chosenLessonsArray = studentObject["ChosenLessons"].toArray();
                for (auto &&j: chosenLessonsArray) {
                    student.ChosenLessons.append(j.toString());
                }
                students.append(student);
            }
        } else {
            // 请求失败，获取并显示错误消息
            QString message = docJson["message"].toString();
            QMessageBox::warning((QWidget *) this, "警告", "获取学生信息失败：" + message);
        }
    }

public slots:

    void doAddRetake() {
        addRetake(lineEdit_Super_Grade_RetakeId->text(), lineEdit_Super_Grade_ToRetakeId->text(),
                  lineEdit_Super_Grade_StudentId->text());
    };

    void doLogout() {
        isLogin = false;
        currentStudent.Id = "";
        currentTeacher.Id = "";
        clearCache();
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

    void fillTableWidget_Teacher_Schedule() {
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 7; j++) {
                auto *item = new QTableWidgetItem();
                item->setTextAlignment(Qt::AlignCenter);
                tableWidget_Teacher_Schedule->setItem(i, j, item);
            }
        }

        for (auto &&lesson: teachingLessons) {
            //检查是否为当前选择学期的课程
            if (lesson.LessonSemester != comboBox_Teacher_Schedule_Semester->currentText()) {
                continue;
            }
            QString lessonString;
            lessonString += lesson.LessonName + "\n";
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
                        appendTextToTeacherTableItem(0, column, lessonStringForOneTimeAndLocation);
                        isAdded[0] = 1;
                    } else if ((classPeriod == "03" || classPeriod == "04" || classPeriod == "05") && isAdded[1] == 0) {
                        appendTextToTeacherTableItem(1, column, lessonStringForOneTimeAndLocation);
                        isAdded[1] = 1;
                    } else if ((classPeriod == "06" || classPeriod == "07") && isAdded[2] == 0) {
                        appendTextToTeacherTableItem(2, column, lessonStringForOneTimeAndLocation);
                        isAdded[2] = 1;
                    } else if ((classPeriod == "08" || classPeriod == "09") && isAdded[3] == 0) {
                        appendTextToTeacherTableItem(3, column, lessonStringForOneTimeAndLocation);
                        isAdded[3] = 1;
                    } else if ((classPeriod == "10" || classPeriod == "11" || classPeriod == "12") && isAdded[4] == 0) {
                        appendTextToTeacherTableItem(4, column, lessonStringForOneTimeAndLocation);
                        isAdded[4] = 1;
                    } else if ((classPeriod == "13" || classPeriod == "14" || classPeriod == "15") && isAdded[5] == 0) {
                        appendTextToTeacherTableItem(5, column, lessonStringForOneTimeAndLocation);
                        isAdded[5] = 1;
                    }
                }
            }
        }
        tableWidget_Teacher_Schedule->resizeRowsToContents();
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
        localStudentsTemp.clear();
        localLessonClassesTemp.clear();
        localLessonsListTemp.clear();
        localTeachersListTemp.clear();
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
            //若为-1则为空
            QString TotalGrade = grade.TotalGrade == -1 ? "" : QString::number(grade.TotalGrade);
            auto *item3 = new QTableWidgetItem(TotalGrade);
            item3->setTextAlignment(Qt::AlignCenter);
            QString RegularGrade = grade.RegularGrade == -1 ? "" : QString::number(grade.RegularGrade);
            auto *item4 = new QTableWidgetItem(RegularGrade);
            item4->setTextAlignment(Qt::AlignCenter);
            QString ExamGrade = grade.ExamGrade == -1 ? "" : QString::number(grade.ExamGrade);
            auto *item5 = new QTableWidgetItem(ExamGrade);
            item5->setTextAlignment(Qt::AlignCenter);
            auto *item6 = new QTableWidgetItem(QString::number(lesson.LessonCredits));
            item6->setTextAlignment(Qt::AlignCenter);
            for (int i = 0; i < grade.RetakeLessonId.size(); i++) {
                RetakeLessons.insert(grade.RetakeLessonId[i], i + 1);
            }
            QString Type;
            if (grade.Retake == 2) {
                Type = "重修" + QString::number(RetakeLessons[lesson.Id] + 1);
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
        }
        tableWidget_Grade->resizeRowsToContents();
    }

    void fillTableWidget_Teacher_Teaching() {
        tableWidget_Teacher_Teaching->setRowCount(0);
        QVector<Lesson> ToBeShownLessons;
        for (auto &&lesson: teachingLessons) {
            //检查是否为当前选择学期的课程
            if (comboBox_Teacher_Teaching_Semester->currentText() == "(全部)" ||
                lesson.LessonSemester == comboBox_Teacher_Teaching_Semester->currentText()) {
                // 检查搜索框是否为空
                if (lineEdit_Teacher_Teaching_Search->text().isEmpty()) {
                    ToBeShownLessons.append(lesson);
                } else {
                    //空格分词
                    QStringList words = lineEdit_Teacher_Teaching_Search->text().split(" ");
                    bool isMatched = true;
                    QVector<QString> classes;
                    listLessonClassesFromLocal(lesson.Id, classes);
                    QString classString;
                    for (auto &&i: classes) {
                        classString += i + ",";
                    }
                    for (auto &&word: words) {
                        if (lesson.Id.contains(word) || lesson.LessonName.contains(word) ||
                            lesson.LessonArea.contains(word) || classString.contains(word)) {
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
        for (auto &&lesson: ToBeShownLessons) {
            // 插入新行
            int row = tableWidget_Teacher_Teaching->rowCount();
            tableWidget_Teacher_Teaching->insertRow(row);

            // 创建表格项并设置文本居中
            auto *item0 = new QTableWidgetItem(lesson.LessonSemester);
            item0->setTextAlignment(Qt::AlignCenter);
            auto *item1 = new QTableWidgetItem(lesson.Id);
            item1->setTextAlignment(Qt::AlignCenter);
            auto *item2 = new QTableWidgetItem(lesson.LessonName);
            item2->setTextAlignment(Qt::AlignCenter);
            auto *item3 = new QTableWidgetItem(lesson.LessonArea);
            item3->setTextAlignment(Qt::AlignCenter);
            QString timeAndLocation;
            for (auto &&pair: lesson.LessonTimeAndLocations.toStdMap()) {
                timeAndLocation += pair.first + " " + pair.second[0] + " " + pair.second[1] + ",";
            }
            timeAndLocation.chop(1);
            auto *item4 = new QTableWidgetItem(timeAndLocation);
            item4->setTextAlignment(Qt::AlignCenter);
            QVector<QString> classes;
            listLessonClassesFromLocal(lesson.Id, classes);
            QString classString;
            for (auto &&i: classes) {
                classString += i + ",";
            }
            classString.chop(1);
            auto *item5 = new QTableWidgetItem(classString);
            item5->setTextAlignment(Qt::AlignCenter);
            auto *item6 = new QTableWidgetItem(QString::number(lesson.LessonStudents.size()));
            item6->setTextAlignment(Qt::AlignCenter);
            auto *label = new QLabel();
            label->setText(QString("<a href='%1'>查看学生</a>").arg(lesson.Id));
            label->setAlignment(Qt::AlignCenter);
            connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::openLessonStudents);

            tableWidget_Teacher_Teaching->setCellWidget(row, 7, label);

            // 将表格项添加到表格中
            tableWidget_Teacher_Teaching->setItem(row, 0, item0);
            tableWidget_Teacher_Teaching->setItem(row, 1, item1);
            tableWidget_Teacher_Teaching->setItem(row, 2, item2);
            tableWidget_Teacher_Teaching->setItem(row, 3, item3);
            tableWidget_Teacher_Teaching->setItem(row, 4, item4);
            tableWidget_Teacher_Teaching->setItem(row, 5, item5);
            tableWidget_Teacher_Teaching->setItem(row, 6, item6);
        }
        tableWidget_Teacher_Teaching->resizeRowsToContents();
    }

    void fillTableWidget_Teacher_Grade() {
        tableWidget_Teacher_Grade->setRowCount(0);
        QVector<Lesson> ToBeShownLessons;
        for (auto &&lesson: teachingLessons) {
            //检查是否为当前选择学期的课程
            if (comboBox_Teacher_Grade_Semester->currentText() == "(全部)" ||
                lesson.LessonSemester == comboBox_Teacher_Grade_Semester->currentText()) {
                // 检查搜索框是否为空
                if (lineEdit_Teacher_Grade_Search->text().isEmpty()) {
                    ToBeShownLessons.append(lesson);
                } else {
                    //空格分词
                    QStringList words = lineEdit_Teacher_Grade_Search->text().split(" ");
                    bool isMatched = true;
                    QVector<QString> classes;
                    listLessonClassesFromLocal(lesson.Id, classes);
                    QString classString;
                    for (auto &&i: classes) {
                        classString += i + ",";
                    }
                    for (auto &&word: words) {
                        if (lesson.Id.contains(word) || lesson.LessonName.contains(word) ||
                            lesson.LessonArea.contains(word) || classString.contains(word)) {
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
        for (auto &&lesson: ToBeShownLessons) {
            // 插入新行
            int row = tableWidget_Teacher_Grade->rowCount();
            tableWidget_Teacher_Grade->insertRow(row);

            // 创建表格项并设置文本居中
            auto *item0 = new QTableWidgetItem(lesson.LessonSemester);
            item0->setTextAlignment(Qt::AlignCenter);
            auto *item1 = new QTableWidgetItem(lesson.Id);
            item1->setTextAlignment(Qt::AlignCenter);
            auto *item2 = new QTableWidgetItem(lesson.LessonName);
            item2->setTextAlignment(Qt::AlignCenter);
            auto *item3 = new QTableWidgetItem(lesson.LessonArea);
            item3->setTextAlignment(Qt::AlignCenter);
            QString timeAndLocation;
            for (auto &&pair: lesson.LessonTimeAndLocations.toStdMap()) {
                timeAndLocation += pair.first + " " + pair.second[0] + " " + pair.second[1] + ",";
            }
            timeAndLocation.chop(1);
            auto *item4 = new QTableWidgetItem(timeAndLocation);
            item4->setTextAlignment(Qt::AlignCenter);
            QVector<QString> classes;
            listLessonClassesFromLocal(lesson.Id, classes);
            QString classString;
            for (auto &&i: classes) {
                classString += i + ",";
            }
            classString.chop(1);
            auto *item5 = new QTableWidgetItem(classString);
            item5->setTextAlignment(Qt::AlignCenter);
            auto *item6 = new QTableWidgetItem(QString::number(lesson.LessonStudents.size()));
            item6->setTextAlignment(Qt::AlignCenter);
            auto *label = new QLabel();
            label->setText(QString("<a href='%1'>录入成绩</a>").arg(lesson.Id));
            label->setAlignment(Qt::AlignCenter);
            connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::openLessonGrades);

            tableWidget_Teacher_Grade->setCellWidget(row, 7, label);

            // 将表格项添加到表格中
            tableWidget_Teacher_Grade->setItem(row, 0, item0);
            tableWidget_Teacher_Grade->setItem(row, 1, item1);
            tableWidget_Teacher_Grade->setItem(row, 2, item2);
            tableWidget_Teacher_Grade->setItem(row, 3, item3);
            tableWidget_Teacher_Grade->setItem(row, 4, item4);
            tableWidget_Teacher_Grade->setItem(row, 5, item5);
            tableWidget_Teacher_Grade->setItem(row, 6, item6);
        }
        tableWidget_Teacher_Grade->resizeRowsToContents();
    }

    void fillTableWidget_Super_Lesson() {
        tableWidget_Super_Lesson->setRowCount(0);
        QVector<Lesson> ToBeShownLessons;
        QVector<Lesson> lessons;
        listLessonsFromLocal(lessons);

        for (auto &&lesson: lessons) {
            //检查是否为当前选择学期的课程
            if (comboBox_Super_Lesson_Semester->currentText() == "(全部)" ||
                lesson.LessonSemester == comboBox_Super_Lesson_Semester->currentText()) {
                // 检查搜索框是否为空
                if (lineEdit_Super_Lesson_Search->text().isEmpty()) {
                    ToBeShownLessons.append(lesson);
                } else {
                    //空格分词
                    QStringList words = lineEdit_Super_Lesson_Search->text().split(" ");
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
        for (auto &&lesson: ToBeShownLessons) {
            // 插入新行
            int row = tableWidget_Super_Lesson->rowCount();
            tableWidget_Super_Lesson->insertRow(row);

            Teacher teacher;
            getTeacherByIdFromLocal(lesson.TeacherId, teacher);
            // 创建表格项并设置文本居中
            auto *item0 = new QTableWidgetItem(lesson.Id);
            item0->setTextAlignment(Qt::AlignCenter);
            item0->setFlags(item0->flags() & ~Qt::ItemIsEditable);
            auto *item1 = new QTableWidgetItem(lesson.LessonName);
            item1->setTextAlignment(Qt::AlignCenter);
            auto *item2 = new QTableWidgetItem(teacher.Id);
            item2->setTextAlignment(Qt::AlignCenter);
            auto *item3 = new QTableWidgetItem(QString::number(lesson.LessonCredits));
            item3->setTextAlignment(Qt::AlignCenter);
            QString timeAndLocation;
            for (auto &&pair: lesson.LessonTimeAndLocations.toStdMap()) {
                timeAndLocation += pair.first + " " + pair.second[0] + " " + pair.second[1] + ",";
            }
            timeAndLocation.chop(1);
            auto *item4 = new QTableWidgetItem(lesson.LessonSemester);
            item4->setTextAlignment(Qt::AlignCenter);
            auto *item5 = new QTableWidgetItem(lesson.LessonArea);
            item5->setTextAlignment(Qt::AlignCenter);
            auto *item6 = new QTableWidgetItem(timeAndLocation);
            item6->setTextAlignment(Qt::AlignCenter);
            auto *label = new QLabel();
            label->setText(QString("<a href='UPDATE/%1/%2'>更新</a> <a href='DELETE/%1/%2'>删除</a>").arg(lesson.Id).arg(row));
            label->setAlignment(Qt::AlignCenter);
            connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::updateOrDeleteLesson);

            tableWidget_Super_Lesson->setCellWidget(row, 7, label);

            // 将表格项添加到表格中
            tableWidget_Super_Lesson->setItem(row, 0, item0);
            tableWidget_Super_Lesson->setItem(row, 1, item1);
            tableWidget_Super_Lesson->setItem(row, 2, item2);
            tableWidget_Super_Lesson->setItem(row, 3, item3);
            tableWidget_Super_Lesson->setItem(row, 4, item4);
            tableWidget_Super_Lesson->setItem(row, 5, item5);
            tableWidget_Super_Lesson->setItem(row, 6, item6);
        }
        tableWidget_Super_Lesson->resizeRowsToContents();
    }

    void fillTableWidget_Super_Lesson_Assign() {
        tableWidget_Super_Lesson_Assign->setRowCount(0);
        QVector<Lesson> ToBeShownLessons;
        //和fillTableWidget_Super_Grade一样
        QVector<Lesson> lessons;
        listLessonsFromLocal(lessons);
        for (auto &&lesson: lessons) {
            //检查是否为当前选择学期的课程
            if (comboBox_Super_Lesson_Assign_Semester->currentText() == "(全部)" ||
                lesson.LessonSemester == comboBox_Super_Lesson_Assign_Semester->currentText()) {
                // 检查搜索框是否为空
                if (lineEdit_Super_Lesson_Assign_Search->text().isEmpty()) {
                    ToBeShownLessons.append(lesson);
                } else {
                    //空格分词
                    QStringList words = lineEdit_Super_Lesson_Assign_Search->text().split(" ");
                    bool isMatched = true;
                    QVector<QString> classes;
                    listLessonClassesFromLocal(lesson.Id, classes);
                    QString classString;
                    for (auto &&i: classes) {
                        classString += i + ",";
                    }
                    for (auto &&word: words) {
                        if (lesson.Id.contains(word) || lesson.LessonName.contains(word) ||
                            lesson.LessonArea.contains(word) || classString.contains(word)) {
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

        for (auto &&lesson: ToBeShownLessons) {
            // 插入新行
            int row = tableWidget_Super_Lesson_Assign->rowCount();
            tableWidget_Super_Lesson_Assign->insertRow(row);

            // 创建表格项并设置文本居中
            auto *item0 = new QTableWidgetItem(lesson.LessonSemester);
            item0->setTextAlignment(Qt::AlignCenter);
            auto *item1 = new QTableWidgetItem(lesson.Id);
            item1->setTextAlignment(Qt::AlignCenter);
            auto *item2 = new QTableWidgetItem(lesson.LessonName);
            item2->setTextAlignment(Qt::AlignCenter);
            auto *item3 = new QTableWidgetItem(lesson.LessonArea);
            item3->setTextAlignment(Qt::AlignCenter);
            QString timeAndLocation;
            for (auto &&pair: lesson.LessonTimeAndLocations.toStdMap()) {
                timeAndLocation += pair.first + " " + pair.second[0] + " " + pair.second[1] + ",";
            }
            timeAndLocation.chop(1);
            auto *item4 = new QTableWidgetItem(timeAndLocation);
            item4->setTextAlignment(Qt::AlignCenter);
            QVector<QString> classes;
            listLessonClassesFromLocal(lesson.Id, classes);
            QString classString;
            for (auto &&i: classes) {
                classString += i + ",";
            }
            classString.chop(1);
            auto *item5 = new QTableWidgetItem(classString);
            item5->setTextAlignment(Qt::AlignCenter);
            auto *item6 = new QTableWidgetItem(QString::number(lesson.LessonStudents.size()));
            item6->setTextAlignment(Qt::AlignCenter);
            auto *label = new QLabel();
            label->setText(QString("<a href='%1'>选课管理</a>").arg(lesson.Id));
            label->setAlignment(Qt::AlignCenter);
            connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::openLessonChosenList);

            tableWidget_Super_Lesson_Assign->setCellWidget(row, 7, label);

            // 将表格项添加到表格中
            tableWidget_Super_Lesson_Assign->setItem(row, 0, item0);
            tableWidget_Super_Lesson_Assign->setItem(row, 1, item1);
            tableWidget_Super_Lesson_Assign->setItem(row, 2, item2);
            tableWidget_Super_Lesson_Assign->setItem(row, 3, item3);
            tableWidget_Super_Lesson_Assign->setItem(row, 4, item4);
            tableWidget_Super_Lesson_Assign->setItem(row, 5, item5);
            tableWidget_Super_Lesson_Assign->setItem(row, 6, item6);
        }
        tableWidget_Super_Lesson_Assign->resizeRowsToContents();
    }

    void openLessonChosenList(const QString &lessonId) {
        Lesson lesson;
        getLessonByIdFromLocal(lessonId, lesson);
        auto *p = new StudentListForm(this);
        p->setWindowTitle("选课学生列表");
        p->lessonStudentList.clear();
        for (auto &&studentId: lesson.LessonStudents) {
            p->lessonStudentList.append(studentId);
        }
        p->showLessonChosenList(lessonId);
    }

    void updateOrDeleteLesson(const QString &link) {
        QStringList parts = link.split("/");
        QString operation = parts[0];
        QString lessonId = parts[1];
        int row = parts[2].toInt();
        if (operation == "UPDATE") {
            Lesson lesson;
            lesson.Id = lessonId;
            lesson.LessonName = tableWidget_Super_Lesson->item(row, 1)->text();
            lesson.TeacherId = tableWidget_Super_Lesson->item(row, 2)->text();
            lesson.LessonCredits = tableWidget_Super_Lesson->item(row, 3)->text().toInt();
            lesson.LessonSemester = tableWidget_Super_Lesson->item(row, 4)->text();
            lesson.LessonArea = tableWidget_Super_Lesson->item(row, 5)->text();
            QString timeAndLocation = tableWidget_Super_Lesson->item(row, 6)->text();
            QStringList timeAndLocationList = timeAndLocation.split(",");
            QMap<QString, QVector<QString>> timeAndLocations;
            for (auto &&i: timeAndLocationList) {
                parts = i.split(" ");
                QString weeks = parts[0];
                timeAndLocations.insert(weeks, {parts[1], parts[2]});
            }
            lesson.LessonTimeAndLocations = timeAndLocations;
            updateLessonInformation(lesson);
            localLessonsListTemp.clear();
            fillTableWidget_Super_Lesson();
        } else if (operation == "DELETE") {
            if (QMessageBox::question(this, "确认", "确定删除该课程吗？") == QMessageBox::Yes) {
                deleteLesson(lessonId);
                localLessonsListTemp.clear();
                tableWidget_Super_Lesson->removeRow(row);
            }
        }
    }

    void fillTableWidget_Super_Grade() {
        tableWidget_Super_Grade->setRowCount(0);
        QVector<Lesson> ToBeShownLessons;
        QVector<Lesson> lessons;
        listLessonsFromLocal(lessons);
        for (auto &&lesson: lessons) {
            //检查是否为当前选择学期的课程
            if (comboBox_Super_Grade_Semester->currentText() == "(全部)" ||
                lesson.LessonSemester == comboBox_Super_Grade_Semester->currentText()) {
                // 检查搜索框是否为空
                if (lineEdit_Super_Grade_Search->text().isEmpty()) {
                    ToBeShownLessons.append(lesson);
                } else {
                    //空格分词
                    QStringList words = lineEdit_Super_Grade_Search->text().split(" ");
                    bool isMatched = true;
                    QVector<QString> classes;
                    listLessonClassesFromLocal(lesson.Id, classes);
                    QString classString;
                    for (auto &&i: classes) {
                        classString += i + ",";
                    }
                    for (auto &&word: words) {
                        if (lesson.Id.contains(word) || lesson.LessonName.contains(word) ||
                            lesson.LessonArea.contains(word) || classString.contains(word)) {
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

        for (auto &&lesson: ToBeShownLessons) {
            // 插入新行
            int row = tableWidget_Super_Grade->rowCount();
            tableWidget_Super_Grade->insertRow(row);

            // 创建表格项并设置文本居中
            auto *item0 = new QTableWidgetItem(lesson.LessonSemester);
            item0->setTextAlignment(Qt::AlignCenter);
            auto *item1 = new QTableWidgetItem(lesson.Id);
            item1->setTextAlignment(Qt::AlignCenter);
            auto *item2 = new QTableWidgetItem(lesson.LessonName);
            item2->setTextAlignment(Qt::AlignCenter);
            auto *item3 = new QTableWidgetItem(lesson.LessonArea);
            item3->setTextAlignment(Qt::AlignCenter);
            QString timeAndLocation;
            for (auto &&pair: lesson.LessonTimeAndLocations.toStdMap()) {
                timeAndLocation += pair.first + " " + pair.second[0] + " " + pair.second[1] + ",";
            }
            timeAndLocation.chop(1);
            auto *item4 = new QTableWidgetItem(timeAndLocation);
            item4->setTextAlignment(Qt::AlignCenter);
            QVector<QString> classes;
            listLessonClassesFromLocal(lesson.Id, classes);
            QString classString;
            for (auto &&i: classes) {
                classString += i + ",";
            }
            classString.chop(1);
            auto *item5 = new QTableWidgetItem(classString);
            item5->setTextAlignment(Qt::AlignCenter);
            auto *item6 = new QTableWidgetItem(QString::number(lesson.LessonStudents.size()));
            item6->setTextAlignment(Qt::AlignCenter);
            auto *label = new QLabel();
            label->setText(QString("<a href='%1'>查录成绩</a>").arg(lesson.Id));
            label->setAlignment(Qt::AlignCenter);
            connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::openLessonGrades);

            tableWidget_Super_Grade->setCellWidget(row, 7, label);

            // 将表格项添加到表格中
            tableWidget_Super_Grade->setItem(row, 0, item0);
            tableWidget_Super_Grade->setItem(row, 1, item1);
            tableWidget_Super_Grade->setItem(row, 2, item2);
            tableWidget_Super_Grade->setItem(row, 3, item3);
            tableWidget_Super_Grade->setItem(row, 4, item4);
            tableWidget_Super_Grade->setItem(row, 5, item5);
            tableWidget_Super_Grade->setItem(row, 6, item6);
        }
        tableWidget_Super_Grade->resizeRowsToContents();
    }

    void fillTableWidget_Super_Student() {
        tableWidget_Super_Student->setRowCount(0);
        QVector<Student> ToBeShownStudents;
        QVector<Student> students;
        listStudentsFromLocal(students);
        for (auto &&student: students) {
            // 检查搜索框是否为空
            if (lineEdit_Super_Student_Search->text().isEmpty()) {
                ToBeShownStudents.append(student);
            } else {
                //空格分词
                QStringList words = lineEdit_Super_Student_Search->text().split(" ");
                //在学号、姓名、班级、学院、专业中匹配，全部满足则加入
                bool isMatched = true;
                for (auto &&word: words) {
                    if (student.Id.contains(word) || student.Name.contains(word) || student.Class.contains(word) ||
                        student.College.contains(word) || student.Major.contains(word) || student.DormitoryArea.contains(
                            word) || student.DormitoryNum.contains(word) || student.PhoneNumber.contains(word)) {
                        continue;
                    } else {
                        isMatched = false;
                        break;
                    }
                }
                if (isMatched) {
                    ToBeShownStudents.append(student);
                }
            }
        }

        for (auto &&student: ToBeShownStudents) {
            // 插入新行
            int row = tableWidget_Super_Student->rowCount();
            tableWidget_Super_Student->insertRow(row);

            // 创建表格项并设置文本居中
            auto *item0 = new QTableWidgetItem(student.Id);
            item0->setTextAlignment(Qt::AlignCenter);
            item0->setFlags(item0->flags() & ~Qt::ItemIsEditable);
            auto *item1 = new QTableWidgetItem(student.Name);
            item1->setTextAlignment(Qt::AlignCenter);
            auto *item2 = new QTableWidgetItem(student.Sex);
            item2->setTextAlignment(Qt::AlignCenter);
            auto *item3 = new QTableWidgetItem(student.College);
            item3->setTextAlignment(Qt::AlignCenter);
            auto *item4 = new QTableWidgetItem(student.Major);
            item4->setTextAlignment(Qt::AlignCenter);
            auto *item5 = new QTableWidgetItem(student.Class);
            item5->setTextAlignment(Qt::AlignCenter);
            auto *item6 = new QTableWidgetItem(QString::number(student.Age));
            item6->setTextAlignment(Qt::AlignCenter);
            auto *item7 = new QTableWidgetItem(student.PhoneNumber);
            item7->setTextAlignment(Qt::AlignCenter);
            auto *item8 = new QTableWidgetItem(student.DormitoryArea);
            item8->setTextAlignment(Qt::AlignCenter);
            auto *item9 = new QTableWidgetItem(student.DormitoryNum);
            item9->setTextAlignment(Qt::AlignCenter);
            auto label = new QLabel();
            label->setText(QString("<a href='UPDATE/%1/%2'>更新</a> <a href='DELETE/%1/%2'>删除</a>").arg(student.Id).arg(row));
            label->setAlignment(Qt::AlignCenter);
            connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::updateOrDeleteStudent);

            tableWidget_Super_Student->setCellWidget(row, 10, label);

            // 将表格项添加到表格中
            tableWidget_Super_Student->setItem(row, 0, item0);
            tableWidget_Super_Student->setItem(row, 1, item1);
            tableWidget_Super_Student->setItem(row, 2, item2);
            tableWidget_Super_Student->setItem(row, 3, item3);
            tableWidget_Super_Student->setItem(row, 4, item4);
            tableWidget_Super_Student->setItem(row, 5, item5);
            tableWidget_Super_Student->setItem(row, 6, item6);
            tableWidget_Super_Student->setItem(row, 7, item7);
            tableWidget_Super_Student->setItem(row, 8, item8);
            tableWidget_Super_Student->setItem(row, 9, item9);


        }
        tableWidget_Super_Student->resizeRowsToContents();
    }

    void fillTableWidget_Super_Teacher() {
        tableWidget_Super_Teacher->setRowCount(0);
        QVector<Teacher> ToBeShownTeachers;
        QVector<Teacher> teachers;
        listTeachersFromLocal(teachers);
        for (auto &&teacher: teachers) {
            // 检查搜索框是否为空
            if (lineEdit_Super_Teacher_Search->text().isEmpty()) {
                ToBeShownTeachers.append(teacher);
            } else {
                //空格分词
                QStringList words = lineEdit_Super_Teacher_Search->text().split(" ");
                //在工号、姓名、性别、学院中匹配，全部满足则加入
                bool isMatched = true;
                for (auto &&word: words) {
                    if (teacher.Id.contains(word) || teacher.Name.contains(word) || teacher.Unit.contains(word)) {
                        continue;
                    } else {
                        isMatched = false;
                        break;
                    }
                }
                if (isMatched) {
                    ToBeShownTeachers.append(teacher);
                }
            }
        }

        for (auto &&teacher: ToBeShownTeachers) {
            // 插入新行
            int row = tableWidget_Super_Teacher->rowCount();
            tableWidget_Super_Teacher->insertRow(row);

            // 创建表格项并设置文本居中
            auto *item0 = new QTableWidgetItem(teacher.Id);
            item0->setTextAlignment(Qt::AlignCenter);
            item0->setFlags(item0->flags() & ~Qt::ItemIsEditable);
            auto *item1 = new QTableWidgetItem(teacher.Name);
            item1->setTextAlignment(Qt::AlignCenter);
            auto *item2 = new QTableWidgetItem(teacher.Unit);
            item2->setTextAlignment(Qt::AlignCenter);
            QString isSuper;
            if (checkIsSUPER(teacher.Id)) {
                isSuper = "是";
            } else {
                isSuper = "否";
            }
            auto *item3 = new QTableWidgetItem(isSuper);
            item3->setTextAlignment(Qt::AlignCenter);
            auto *label = new QLabel();
            label->setText(QString("<a href='UPDATE/%1/%2'>更新</a> <a href='DELETE/%1/%2'>删除</a>").arg(teacher.Id).arg(row));
            label->setAlignment(Qt::AlignCenter);
            connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::updateOrDeleteTeacher);

            tableWidget_Super_Teacher->setCellWidget(row, 4, label);

            // 将表格项添加到表格中
            tableWidget_Super_Teacher->setItem(row, 0, item0);
            tableWidget_Super_Teacher->setItem(row, 1, item1);
            tableWidget_Super_Teacher->setItem(row, 2, item2);
            tableWidget_Super_Teacher->setItem(row, 3, item3);
        }
        tableWidget_Super_Teacher->resizeRowsToContents();
    }

    void updateOrDeleteTeacher(const QString &link) {
        QStringList linkList = link.split("/");
        QString operation = linkList[0];
        QString teacherId = linkList[1];
        int row = linkList[2].toInt();
        if (operation == "UPDATE") {
            Teacher teacher;
            teacher.Id = tableWidget_Super_Teacher->item(row, 0)->text();
            teacher.Name = tableWidget_Super_Teacher->item(row, 1)->text();
            teacher.Unit = tableWidget_Super_Teacher->item(row, 2)->text();
            updateTeacherInformation(teacher);
            Auth auth;
            auth.Account = teacher.Id;
            auth.AccountType = TEACHER;
            if (tableWidget_Super_Teacher->item(row, 1)->text().isEmpty()) {
                QMessageBox::information(this, "提示", "姓名不能为空");
                return;
            }
            if (tableWidget_Super_Teacher->item(row, 3)->text() != "是" &&
                tableWidget_Super_Teacher->item(row, 3)->text() != "否") {
                QMessageBox::information(this, "提示", "是否管理员只能为是或否");
                return;
            }
            if (tableWidget_Super_Teacher->item(row, 3)->text() == "是") {
                auth.IsSuper = 1;
            } else {
                auth.IsSuper = 0;
            }
            updateAccount(auth);
            localTeachersListTemp.clear();
            fillTableWidget_Super_Teacher();
        } else if (operation == "DELETE") {
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "删除教师", "确定删除工号为" + teacherId + "的教师吗？",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                deleteTeacher(teacherId);
                QMessageBox::information(this, "提示", "删除成功");
                localTeachersListTemp.clear();
                fillTableWidget_Super_Teacher();
            }
        }
    }

    void addLessonToTableWidget_Super_Lesson() {
        tableWidget_Super_Lesson->setRowCount(tableWidget_Super_Lesson->rowCount() + 1);
        int row = tableWidget_Super_Lesson->rowCount() - 1;
        for (int i = 0; i < 7; i++) {
            auto *item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            tableWidget_Super_Lesson->setItem(row, i, item);
        }
        auto label = new QLabel();
        label->setText(QString("<a href='ADD/%1'>提交</a> <a href='CANCEL/%1'>取消</a>").arg(row));
        label->setAlignment(Qt::AlignCenter);
        connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::addOrCancelLesson);
        tableWidget_Super_Lesson->setCellWidget(row, 7, label);
    }

    void addOrCancelLesson(const QString &link) {
        QStringList linkList = link.split("/");
        QString operation = linkList[0];
        int row = linkList[1].toInt();
        if (operation == "ADD") {
            Lesson lesson;
            lesson.Id = tableWidget_Super_Lesson->item(row, 0)->text();
            lesson.LessonName = tableWidget_Super_Lesson->item(row, 1)->text();
            lesson.TeacherId = tableWidget_Super_Lesson->item(row, 2)->text();
            lesson.LessonCredits = tableWidget_Super_Lesson->item(row, 3)->text().toInt();
            lesson.LessonSemester = tableWidget_Super_Lesson->item(row, 4)->text();
            lesson.LessonArea = tableWidget_Super_Lesson->item(row, 5)->text();
            QString timeAndLocation = tableWidget_Super_Lesson->item(row, 6)->text();
            QStringList timeAndLocationList = timeAndLocation.split(",");
            QMap<QString, QVector<QString>> timeAndLocations;
            for (auto &&i: timeAndLocationList) {
                QStringList parts = i.split(" ");
                QString weeks = parts[0];
                timeAndLocations.insert(weeks, {parts[1], parts[2]});
            }
            lesson.LessonTimeAndLocations = timeAndLocations;
            updateLessonInformation(lesson);
            localLessonsListTemp.clear();
            fillTableWidget_Super_Lesson();
        } else if (operation == "CANCEL") {
            tableWidget_Super_Lesson->removeRow(row);
        }
    }

    void addTeacherToTableWidget_Super_Teacher() {
        tableWidget_Super_Teacher->setRowCount(tableWidget_Super_Teacher->rowCount() + 1);
        int row = tableWidget_Super_Teacher->rowCount() - 1;
        for (int i = 0; i < 4; i++) {
            auto *item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            tableWidget_Super_Teacher->setItem(row, i, item);
        }
        auto label = new QLabel();
        label->setText(QString("<a href='ADD/%1'>提交</a> <a href='CANCEL/%1'>取消</a>").arg(row));
        label->setAlignment(Qt::AlignCenter);
        connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::addOrCancelTeacher);
        tableWidget_Super_Teacher->setCellWidget(row, 4, label);
    }

    void addOrCancelTeacher(const QString &link) {
        QStringList linkList = link.split("/");
        QString operation = linkList[0];
        int row = linkList[1].toInt();
        if (operation == "ADD") {
            Teacher teacher;
            teacher.Id = tableWidget_Super_Teacher->item(row, 0)->text();
            teacher.Name = tableWidget_Super_Teacher->item(row, 1)->text();
            teacher.Unit = tableWidget_Super_Teacher->item(row, 2)->text();
            updateTeacherInformation(teacher);
            Auth auth;
            auth.Account = teacher.Id;
            auth.AccountType = TEACHER;
            if (tableWidget_Super_Teacher->item(row, 1)->text().isEmpty()) {
                QMessageBox::information(this, "提示", "姓名不能为空");
                return;
            }
            if (tableWidget_Super_Teacher->item(row, 3)->text() != "是" &&
                tableWidget_Super_Teacher->item(row, 3)->text() != "否") {
                QMessageBox::information(this, "提示", "是否管理员只能为是或否");
                return;
            }
            if (tableWidget_Super_Teacher->item(row, 3)->text() == "是") {
                auth.IsSuper = 1;
            } else {
                auth.IsSuper = 0;
            }
            addAccount(auth, teacher.Id);
            localTeachersListTemp.clear();
            fillTableWidget_Super_Teacher();
        } else if (operation == "CANCEL") {
            tableWidget_Super_Teacher->removeRow(row);
        }
    }

    void addStudentToTableWidget_Super_Student() {
        tableWidget_Super_Student->setRowCount(tableWidget_Super_Student->rowCount() + 1);
        int row = tableWidget_Super_Student->rowCount() - 1;
        for (int i = 0; i < 10; i++) {
            auto *item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            tableWidget_Super_Student->setItem(row, i, item);
        }
        auto label = new QLabel();
        label->setText(QString("<a href='ADD/%1'>提交</a> <a href='CANCEL/%1'>取消</a>").arg(row));
        label->setAlignment(Qt::AlignCenter);
        connect(label, &QLabel::linkActivated, this, &AIMSMainWindow::addOrCancelStudent);
        tableWidget_Super_Student->setCellWidget(row, 10, label);


    }

    void addOrCancelStudent(const QString &link) {
        QStringList linkList = link.split("/");
        QString operation = linkList[0];
        int row = linkList[1].toInt();
        if (operation == "ADD") {
            Student student;
            student.Id = tableWidget_Super_Student->item(row, 0)->text();
            student.Name = tableWidget_Super_Student->item(row, 1)->text();
            student.Sex = tableWidget_Super_Student->item(row, 2)->text();
            if (student.Sex != "男" && student.Sex != "女" && student.Sex != "未知" && student.Sex != "其他") {
                QMessageBox::information(this, "提示", "性别只能为男、女、未知或其他");
                return;
            }
            student.College = tableWidget_Super_Student->item(row, 3)->text();
            student.Major = tableWidget_Super_Student->item(row, 4)->text();
            student.Class = tableWidget_Super_Student->item(row, 5)->text();
            student.Age = tableWidget_Super_Student->item(row, 6)->text().toInt();
            student.PhoneNumber = tableWidget_Super_Student->item(row, 7)->text();
            student.DormitoryArea = tableWidget_Super_Student->item(row, 8)->text();
            student.DormitoryNum = tableWidget_Super_Student->item(row, 9)->text();
            updateStudentInformation(student);
            Auth auth;
            auth.Account = student.Id;
            auth.AccountType = STUDENT;
            if (tableWidget_Super_Student->item(row, 1)->text().isEmpty()) {
                QMessageBox::information(this, "提示", "姓名不能为空");
                return;
            }
            auth.IsSuper = 0;
            addAccount(auth, student.Id);
            localStudentsListTemp.clear();
            fillTableWidget_Super_Student();
        } else if (operation == "CANCEL") {
            tableWidget_Super_Student->removeRow(row);
        }
    }


    void updateOrDeleteStudent(const QString &link) {
        QStringList linkList = link.split("/");
        QString operation = linkList[0];
        QString studentId = linkList[1];
        int row = linkList[2].toInt();
        if (operation == "UPDATE") {
            Student student;
            student.Id = tableWidget_Super_Student->item(row, 0)->text();
            student.Name = tableWidget_Super_Student->item(row, 1)->text();
            student.Sex = tableWidget_Super_Student->item(row, 2)->text();
            if (student.Sex != "男" && student.Sex != "女" && student.Sex != "未知" && student.Sex != "其他") {
                QMessageBox::information(this, "提示", "性别只能为男、女、未知或其他");
                return;
            }
            student.College = tableWidget_Super_Student->item(row, 3)->text();
            student.Major = tableWidget_Super_Student->item(row, 4)->text();
            student.Class = tableWidget_Super_Student->item(row, 5)->text();
            student.Age = tableWidget_Super_Student->item(row, 6)->text().toInt();
            student.PhoneNumber = tableWidget_Super_Student->item(row, 7)->text();
            student.DormitoryArea = tableWidget_Super_Student->item(row, 8)->text();
            student.DormitoryNum = tableWidget_Super_Student->item(row, 9)->text();
            updateStudentInformation(student);
            localStudentsListTemp.clear();
            fillTableWidget_Super_Student();
        } else if (operation == "DELETE") {
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "删除学生", "确定删除编号为" + studentId + "的学生吗？",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                deleteStudent(studentId);
                QMessageBox::information(this, "提示", "删除成功");
                localStudentsListTemp.clear();
                fillTableWidget_Super_Student();
            }
        }
    }

    void openLessonStudents(const QString &lessonId) const {
        //QMessageBox::information(this, "提示", "查看学生：" + lessonId);
        StudentListForm *StudentListForm;
        StudentListForm = new class StudentListForm((QMainWindow *) this);
        StudentListForm->showLessonStudentList(lessonId);
    }

    void openLessonGrades(const QString &lessonId) const {
        //QMessageBox::information(this, "提示", "录入成绩：" + lessonId);
        StudentListForm *StudentListForm;
        StudentListForm = new class StudentListForm((QMainWindow *) this);
        StudentListForm->showLessonGradeList(lessonId);
    }

    void showChangePasswordForm() {
        ChangePasswordForm *changePasswordForm;
        changePasswordForm = new class ChangePasswordForm((QMainWindow *) this);
        changePasswordForm->show();
    }


signals:
protected:
    void resizeEvent(QResizeEvent *event) override {
        QMainWindow::resizeEvent(event);  // 调用父类的resizeEvent函数
        resizeWidget();
    }
};

void StudentListForm::fillTableWidget_LessonStudent() {
    tableWidget_LessonStudent->setRowCount(0);
    Lesson lesson;
    auto *parentAIMSMainWindow = (AIMSMainWindow *) this->parentWidget();
    parentAIMSMainWindow->getLessonByIdFromLocal(currentLessonId, lesson);

    for (auto &&studentId: lesson.LessonStudents) {
        Student student;
        parentAIMSMainWindow->getStudentByIdFromLocal(studentId, student);
        Grade grade;
        parentAIMSMainWindow->getStudentLessonGradeFromLocal(studentId, currentLessonId, grade);
        QString retake;
        if (grade.Retake == 2) {
            retake = "重修";
        } else {
            retake = "正常考试";
        }
        if (!lineEdit_Search_LessonStudent->text().isEmpty()) {
            QStringList searchWords = lineEdit_Search_LessonStudent->text().split(" ");
            bool isMatched = true;
            for (auto &&word: searchWords) {

                if (student.Id.contains(word) || student.Name.contains(word) || student.Class.contains(word) ||
                    student.College.contains(word) || student.Major.contains(word) || retake.contains(word)) {
                    continue;
                } else {
                    isMatched = false;
                    break;
                }
            }
            if (!isMatched) {
                continue;
            }
        }

        // 插入新行
        int row = tableWidget_LessonStudent->rowCount();
        tableWidget_LessonStudent->insertRow(row);

        // 创建表格项并设置文本居中
        auto *item0 = new QTableWidgetItem(student.Id);
        item0->setTextAlignment(Qt::AlignCenter);
        auto *item1 = new QTableWidgetItem(student.Name);
        item1->setTextAlignment(Qt::AlignCenter);
        auto *item2 = new QTableWidgetItem(student.Class);
        item2->setTextAlignment(Qt::AlignCenter);
        auto *item3 = new QTableWidgetItem(student.College);
        item3->setTextAlignment(Qt::AlignCenter);
        auto *item4 = new QTableWidgetItem(student.Major);
        item4->setTextAlignment(Qt::AlignCenter);
        auto *item5 = new QTableWidgetItem(retake);
        item5->setTextAlignment(Qt::AlignCenter);

        // 将表格项添加到表格中
        tableWidget_LessonStudent->setItem(row, 0, item0);
        tableWidget_LessonStudent->setItem(row, 1, item1);
        tableWidget_LessonStudent->setItem(row, 2, item2);
        tableWidget_LessonStudent->setItem(row, 3, item3);
        tableWidget_LessonStudent->setItem(row, 4, item4);
        tableWidget_LessonStudent->setItem(row, 5, item5);

        QVector<int> Scale;
        Scale << 15 << 10 << 10 << 15 << 20 << 10;
        for (int i = 0; i < 6; i++) {
            int width = (tableWidget_LessonStudent->size().width()) * Scale[i] / (15 + 10 + 10 + 15 + 20 + 10);
            tableWidget_LessonStudent->setColumnWidth(i, width);
        }
    }
    tableWidget_LessonStudent->resizeRowsToContents();
}

void StudentListForm::fillTableWidget_LessonGrade() {
    disconnect(tableWidget_LessonGrade, &QTableWidget::itemChanged, this, &StudentListForm::doCheckAndSendGrade);
    tableWidget_LessonGrade->setRowCount(0);
    lastClickedStudentId = "";
    Lesson lesson;
    auto *parentAIMSMainWindow = (AIMSMainWindow *) this->parentWidget();
    parentAIMSMainWindow->getLessonByIdFromLocal(currentLessonId, lesson);

    for (auto &&studentId: lesson.LessonStudents) {
        Student student;
        parentAIMSMainWindow->getStudentByIdFromLocal(studentId, student);
        Grade grade;
        parentAIMSMainWindow->getStudentLessonGradeFromLocal(studentId, currentLessonId, grade);
        QString retake;
        if (grade.Retake == 2) {
            retake = "重修";
        } else {
            retake = "正常考试";
        }
        if (!lineEdit_Search_LessonGrade->text().isEmpty()) {
            QStringList searchWords = lineEdit_Search_LessonGrade->text().split(" ");
            bool isMatched = true;
            for (auto &&word: searchWords) {

                if (student.Id.contains(word) || student.Name.contains(word) || student.Class.contains(word) ||
                    student.College.contains(word) || student.Major.contains(word) || retake.contains(word)) {
                    continue;
                } else {
                    isMatched = false;
                    break;
                }
            }
            if (!isMatched) {
                continue;
            }
        }

        // 插入新行
        int row = tableWidget_LessonGrade->rowCount();
        tableWidget_LessonGrade->insertRow(row);

        // 创建表格项并设置文本居中
        auto *item0 = new QTableWidgetItem(student.Id);
        item0->setTextAlignment(Qt::AlignCenter);
        item0->setFlags(item0->flags() & (~Qt::ItemIsEditable));
        auto *item1 = new QTableWidgetItem(student.Name);
        item1->setTextAlignment(Qt::AlignCenter);
        item1->setFlags(item1->flags() & (~Qt::ItemIsEditable));
        auto *item2 = new QTableWidgetItem(student.Class);
        item2->setTextAlignment(Qt::AlignCenter);
        item2->setFlags(item2->flags() & (~Qt::ItemIsEditable));
        auto *item3 = new QTableWidgetItem(grade.TotalGrade == -1 ? "" : QString::number(grade.TotalGrade));
        item3->setTextAlignment(Qt::AlignCenter);
        auto *item4 = new QTableWidgetItem(grade.RegularGrade == -1 ? "" : QString::number(grade.RegularGrade));
        item4->setTextAlignment(Qt::AlignCenter);
        auto *item5 = new QTableWidgetItem(grade.ExamGrade == -1 ? "" : QString::number(grade.ExamGrade));
        item5->setTextAlignment(Qt::AlignCenter);
        auto *item6 = new QTableWidgetItem(retake);
        item6->setTextAlignment(Qt::AlignCenter);
        item6->setFlags(item6->flags() & (~Qt::ItemIsEditable));

        // 将表格项添加到表格中
        tableWidget_LessonGrade->setItem(row, 0, item0);
        tableWidget_LessonGrade->setItem(row, 1, item1);
        tableWidget_LessonGrade->setItem(row, 2, item2);
        tableWidget_LessonGrade->setItem(row, 3, item3);
        tableWidget_LessonGrade->setItem(row, 4, item4);
        tableWidget_LessonGrade->setItem(row, 5, item5);
        tableWidget_LessonGrade->setItem(row, 6, item6);

        QVector<int> Scale;
        Scale << 15 << 10 << 10 << 10 << 10 << 10 << 10;
        for (int i = 0; i < 7; i++) {
            int width = (tableWidget_LessonGrade->size().width()) * Scale[i] / (15 + 10 + 10 + 10 + 10 + 10 + 10);
            tableWidget_LessonGrade->setColumnWidth(i, width);
        }
    }
    tableWidget_LessonGrade->resizeRowsToContents();
    connect(tableWidget_LessonGrade, &QTableWidget::itemChanged, this, &StudentListForm::doCheckAndSendGrade);
}

void StudentListForm::doCheckAndSendGrade() {
    if (lastClickedStudentId.isEmpty()) {
        return;
    }
    QString studentId = lastClickedStudentId;

    //获取当前表格成绩
    Grade localGrade;
    localGrade.StudentId = studentId;
    localGrade.LessonId = currentLessonId;
    //为空字符串则为-1
    if (tableWidget_LessonGrade->item(lastClickedCell.first, 5)->text().isEmpty()) {
        localGrade.ExamGrade = -2;
    } else {
        localGrade.ExamGrade = tableWidget_LessonGrade->item(lastClickedCell.first, 5)->text().toDouble();
    }
    if (tableWidget_LessonGrade->item(lastClickedCell.first, 4)->text().isEmpty()) {
        localGrade.RegularGrade = -2;
    } else {
        localGrade.RegularGrade = tableWidget_LessonGrade->item(lastClickedCell.first, 4)->text().toDouble();
    }
    if (tableWidget_LessonGrade->item(lastClickedCell.first, 3)->text().isEmpty()) {
        localGrade.TotalGrade = -2;
    } else {
        localGrade.TotalGrade = tableWidget_LessonGrade->item(lastClickedCell.first, 3)->text().toDouble();
    }

    auto *parentAIMSMainWindow = (AIMSMainWindow *) this->parentWidget();
    parentAIMSMainWindow->updateStudentLessonGrade(localGrade);
    parentAIMSMainWindow->localGradesTemp.clear();
}

void ChangePasswordForm::changePassword() {
    if (lineEdit_NewPassword->text().isEmpty() || lineEdit_ConfirmPassword->text().isEmpty()) {
        QMessageBox::warning(this, "警告", "新密码或确认密码不能为空");
        return;
    }

    if (lineEdit_NewPassword->text() != lineEdit_ConfirmPassword->text()) {
        QMessageBox::warning(this, "警告", "新密码和确认密码不一致");
        return;
    }

    newPassword = lineEdit_NewPassword->text();

    auto *parentAIMSMainWindow = (AIMSMainWindow *) this->parentWidget();
    parentAIMSMainWindow->changePassword(newPassword);

    close();
}

void StudentListForm::fillTableWidget_LessonChosen() {
    auto *parentAIMSMainWindow = (AIMSMainWindow *) this->parentWidget();
    tableWidget_LessonChosenStudent->setRowCount(0);
    for (auto &i: lessonStudentList) {
        tableWidget_LessonChosenStudent->insertRow(tableWidget_LessonChosenStudent->rowCount());
        auto *item0 = new QTableWidgetItem(i);
        item0->setTextAlignment(Qt::AlignCenter);
        tableWidget_LessonChosenStudent->setItem(tableWidget_LessonChosenStudent->rowCount() - 1, 0, item0);
    }
    tableWidget_LessonChosenStudent->resizeRowsToContents();
}

void StudentListForm::finishChoosing() {
    qDebug() << "finishChoosing";
    auto *parentAIMSMainWindow = (AIMSMainWindow *) this->parentWidget();
    for (int i = 0; i < tableWidget_LessonChosenStudent->rowCount(); i++) {
        QString studentId = tableWidget_LessonChosenStudent->item(i, 0)->text();
        if (!lessonStudentList.contains(studentId)) {
            parentAIMSMainWindow->addChosenLesson(studentId, currentLessonId);
        }
    }
    //在lessonStudentList中的学生不在tableWidget_LessonChosenStudent中，删除
    for (auto &i: lessonStudentList) {
        bool isExist = false;
        for (int j = 0; j < tableWidget_LessonChosenStudent->rowCount(); j++) {
            if (i == tableWidget_LessonChosenStudent->item(j, 0)->text()) {
                isExist = true;
                break;
            }
        }
        if (!isExist) {
            parentAIMSMainWindow->deleteChosenLesson(i, currentLessonId);
        }
    }
    parentAIMSMainWindow->localLessonsListTemp.clear();
    parentAIMSMainWindow->localLessonClassesTemp.clear();
    parentAIMSMainWindow->localLessonsTemp.clear();
    parentAIMSMainWindow->fillTableWidget_Super_Lesson_Assign();
    close();

}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    AIMSMainWindow mainWindow;
    mainWindow.doLogin();

    return QApplication::exec();
}

#include "main.moc"