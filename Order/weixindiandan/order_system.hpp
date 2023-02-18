//
// Created by rkasasa on 23-1-19.
//

#ifndef ORDER_ORDER_SYSTEM_HPP
#define ORDER_ORDER_SYSTEM_HPP

#include <iostream>
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
#include "muduo/base/Mutex.h"
namespace order_system
{
    static MYSQL* MySQLInit()
    {
        MYSQL* connect_fd = mysql_init(NULL);
        if(mysql_real_connect(connect_fd,"127.0.0.1","root","142724","order_system",3306,NULL,0)==NULL)
        {
            printf("连接失败！ %s\n",mysql_error(connect_fd));
            return NULL;
        }
        mysql_set_character_set(connect_fd,"utf8");
        return connect_fd;
    }
    static void MySQLRelease(MYSQL* mysql)
    {
        mysql_close(mysql);
    }

    class DishTable
    {
    public:
        muduo::MutexLock mutex_;
        DishTable(MYSQL* mysql):mysql_(mysql){}
        bool Insert(const Json::Value &dish)
        {
            char sql[1024]={0};
            sprintf(sql, "insert into dish_table values(null, '%s', %d,%d)", dish["name"].asCString(),
                    dish["price"].asInt(),dish["number"].asInt());
            int ret = mysql_query(mysql_,sql);
            if(ret!=0)
            {
                printf("执行 sql 失败! sql=%s, %s\n", sql, mysql_error(mysql_));
                return false;
            }

            return true;
        }

        bool SelectAll(Json::Value* dishes) {
            char sql[1024] = {0};
            sprintf(sql, "select * from dish_table");
            int ret = mysql_query(mysql_, sql);
            if (ret != 0) {
                printf("执行 sql 失败! %s\n", mysql_error(mysql_));
                return false;
            }
            MYSQL_RES* result = mysql_store_result(mysql_);
            if (result == NULL) {
                printf("获取结果失败! %s\n", mysql_error(mysql_));
                return false;
            }
            int rows= mysql_num_rows(result);
            for(int i=0;i<rows;i++)
            {
                MYSQL_ROW row = mysql_fetch_row(result);
                Json::Value dish;
                dish["dish_id"]=atoi(row[0]);
                dish["name"]=row[1];
                dish["price"]=atoi(row[2]);
                dish["number"]=atoi(row[3]);
                dishes->append(dish);
            }
            return true;
        }

        bool SelectOne(int dish_id,Json::Value* dish)
        {
            char sql[1024 * 4] = {0};
            sprintf(sql, "select * from dish_table where dish_id = %d", dish_id);
            int ret = mysql_query(mysql_, sql);
            if (ret != 0) {
                printf("执行 sql 失败! %s\n", mysql_error(mysql_));
                return false;
            }
            MYSQL_RES* result = mysql_store_result(mysql_);
            if (result == NULL) {
                printf("获取结果失败! %s\n", mysql_error(mysql_));
                return false;
            }
            int rows = mysql_num_rows(result);
            if (rows != 1) {
                printf("查找结果不为 1 条. rows = %d!\n", rows);
                return false;
            }

            MYSQL_ROW row = mysql_fetch_row(result);
            (*dish)["dish_id"] = atoi(row[0]);
            (*dish)["name"] = row[1];
            (*dish)["price"] = atoi(row[2]);
            (*dish)["number"] = atoi(row[3]);
            return true;
        }

        bool Update(const Json::Value &dish)
        {
            char sql[1024 * 4] = {0};
            sprintf(sql, "update dish_table SET name='%s', price=%d , number=%d where dish_id=%d",
                    dish["name"].asCString(),
                    dish["price"].asInt(),dish["number"].asInt(), dish["dish_id"].asInt());
            int ret = mysql_query(mysql_, sql);
            if (ret != 0) {
                printf("执行 sql 失败! sql=%s, %s\n", sql, mysql_error(mysql_));
                return false;
            }
            return true;
        }
        bool Update_number(int dish_id,int dish_number)
        {
            char sql[1024 * 4] = {0};
            sprintf(sql, "update dish_table SET number = number-%d where dish_id=%d",
                    dish_number,dish_id);
            int ret = mysql_query(mysql_, sql);
            if (ret != 0) {
                printf("执行 sql 失败! sql=%s, %s\n", sql, mysql_error(mysql_));
                return false;
            }
            return true;
        }
        bool Delete(int dish_id)
        {
            char sql[1024 * 4] = {0};
            sprintf(sql, "delete from dish_table where dish_id=%d", dish_id);
            int ret = mysql_query(mysql_, sql);
            if (ret != 0) {
                printf("执行 sql 失败! sql=%s, %s\n", sql, mysql_error(mysql_));
                return false;
            }
            return true;
        }
    private:
        MYSQL* mysql_;
    };


    class OrderTable
    {
    public:
        OrderTable(MYSQL* mysql):mysql_(mysql){}
        bool SelectAll(Json::Value* orders) {
            char sql[1024 * 4] = {0};
            sprintf(sql, "select * from order_table");
            int ret = mysql_query(mysql_, sql);
            if (ret != 0) {
                printf("执行 sql 失败! %s\n", mysql_error(mysql_));
                return false;
            }
            MYSQL_RES *result = mysql_store_result(mysql_);
            if (result == NULL) {
                printf("获取结果失败! %s\n", mysql_error(mysql_));
                return false;
            }
            int rows = mysql_num_rows(result);
            for (int i = 0; i < rows; ++i) {
                MYSQL_ROW row = mysql_fetch_row(result);
                Json::Value order;
                order["order_id"] = atoi(row[0]); // 注意, order_id 是数字, table_id 是字符串
                order["table_id"] = row[1];
                order["time"] = row[2];
                order["dish_ids"] = row[3];
                order["state"] = row[4];
                orders->append(order);
            }
            return true;
        }

        bool Insert(const Json::Value& order) {
            char sql[1024 * 4] = {0};
            sprintf(sql, "insert into order_table values(null, '%s', '%s', '%s', %d)",
                    order["table_id"].asCString(), order["time"].asCString(),
                    order["dish_ids_str"].asCString(), order["state"].asInt());

            int ret = mysql_query(mysql_, sql);
            if (ret != 0) {
                printf("执行 sql 失败! sql=%s, %s\n", sql, mysql_error(mysql_));
                return false;
            }
            return true;
        }

        bool ChangeState(const Json::Value& order) {
            char sql[1024 * 4] = {0};
            sprintf(sql, "update order_table set state = %d where order_id = %d",
                    order["state"].asInt(), order["order_id"].asInt());
            int ret = mysql_query(mysql_, sql);
            if (ret != 0) {
                printf("执行 sql 失败! sql=%s, %s\n", sql, mysql_error(mysql_));
                return false;
            }
            return true;
        }

    private:
        MYSQL* mysql_;
    };
}

#endif //ORDER_ORDER_SYSTEM_HPP
