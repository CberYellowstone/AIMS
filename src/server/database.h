#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QtSql/QSqlDatabase>
#include <QList>
#include <QMap>

typedef int Status;
#define Success 0
#define ERROR 1
#define NOT_FOUND 2
#define DUPLICATE 3
#define INVALID 4
#define NO_PERMISSION 5
#define RELATION_ERROR 6
#define TEACHER_NOT_FOUND 7
#define STUDENT_NOT_FOUND 8
#define LESSON_NOT_FOUND 9

#define TEACHER 0
#define STUDENT 1
#define SUPER 2
#define EVERYONE 3

#define NOT_RETAKE 0
#define RETAKE 1
#define RETAKEN 2

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
    QString LessonSemester; // 课程学期
    QString LessonArea; // 课程上课区域
    QMap<QString, QVector<QString>> LessonTimeAndLocations; // 课程上课时间和地点
    QVector<QString> LessonStudents; // 选课学生学号
};

class Teacher {
public:
    QString Id; // 教师编号
    QString Name; // 教师
    QVector<QString> TeachingLessons; // 教师教授课程编号
};

class Auth {
public:
    QString Account; // 用户账号
    QString Secret; // 用户密钥
    int AccountType; // 用户类型，0为教师，1为学生
    int IsSuper; // 是否为超级用户，0为否，1为是
};

namespace Database {

    class database {
    public:
        explicit database(const QString &path);

        Status getStudentById(const QString &id, Student &student);

        Status updateStudent(const Student &student);

        Status updateLessonInformation(const Lesson &lesson);

        Status getLessonById(const QString &id, Lesson &lesson);

        Status getTeacherById(const QString &id, Teacher &teacher);

        Status updateTeacher(const Teacher &teacher);

        Status updateTeachingLessons(const QString &teacherId, const QVector<QString> &teachingLessons);

        Status addTeachingLesson(const QString &teacherId, const QString &lessonId);

        Status deleteStudent(const QString &id);

        Status listStudents(QVector<Student> &students, int maximum, int pageNum);

        int getStudentCount();

        Status listTeachers(QVector<Teacher> &teachers, int maximum, int pageNum);

        int getTeacherCount();

        int getLessonCount();

        Status listLessons(QVector<Lesson> &lessons, int maximum, int pageNum);

        Status verifyAccount(const QString &account, const QString &secret, Auth &auth);

        Status updateAccount(const Auth &auth);

        Status createAccount(const Auth &auth);

    private:
        QSqlDatabase db;

        Status initializeDatabase();

        bool ifTableExist(const QString &tableName);

        Status createTableIfNotExists(const QString &tableName);

        Status deleteTeachingLesson(const QString &teacherId, const QString &lessonId);

        Status deleteLesson(const QString &id);

        Status deleteTeacher(const QString &id);

        Status getStudentByClass(const QString &studentClass, QVector<Student> &students);

        Status checkDatabase();

        Status deleteChosenLesson(const QString &studentId, const QString &lessonId);

        int getAuthCount();

        Status ifTeacherExist(const QString &teacherId);

        Status listAuths(QVector<Auth> &auths, int maximum, int pageNum);

        Status deleteAccount(const QString &account);

        Status getAccount(const QString &account, Auth &auth);

    };

} // Database

#endif //DATABASE_H