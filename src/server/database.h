#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QList>

typedef int Status;
#define Success 0
#define ERROR 1

class Student {
public:
    QString Id; // 学生学号
    QString Name; // 学生姓名
    QString Sex; // 学生性别
    QString College; // 学生所在学院
    QString Major; // 学生专业
    QString Class; // 学生班级
    int Age; // 学生年龄
    QString PhoneNumber; // 学生电话号码
    QString DormitoryArea; // 学生所在宿舍区
    QString DormitoryNum; // 学生宿舍号码
    QVector<QString> ChosenLessons; // 学生已选课程，课程编号
};

namespace Database {

    class database {
    public:
        explicit database(const QString &path);

        static Status getStudentById(const QString &id, Student &student);

        static Status updateStudent(const Student &student);

    private:
        QSqlDatabase db;

        Status initializeDatabase();
    };

} // Database

#endif //DATABASE_H