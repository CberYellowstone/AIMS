#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QList>
#include <QMap>

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
    QVector<QString> ChosenLessons; // 学生已选课程编号
};

class Lesson {
public:
    QString Id; // 课程编号
    QString LessonName; // 课程名称
    QString TeacherId; // 课程教师编号
    int LessonCredits; // 课程学分
    QString LessonArea; // 课程上课区域
    QMap<QString, QVector<QString>> LessonTimeAndLocations; // 课程上课时间和地点
    QVector<QString> LessonStudents; // 选课学生学号
};

namespace Database {

    class database {
    public:
        explicit database(const QString &path);

        static Status getStudentById(const QString &id, Student &student);

        static Status updateStudent(const Student &student);

        static Status updateLessonInformation(const Lesson &lesson);

        static Status getLessonById(const QString &id, Lesson &lesson);

    private:
        QSqlDatabase db;

        static Status initializeDatabase();

        static bool ifTableExist(const QString &tableName);

        static Status createTableIfNotExists(const QString &tableName);

        static Status updateTeachingLessons(const QString &teacherId, const QVector<QString> &teachingLessons);

        static Status addTeachingLesson(const QString &teacherId, const QString &lessonId);

        static Status deleteTeachingLesson(const QString &teacherId, const QString &lessonId);
    };

} // Database

#endif //DATABASE_H